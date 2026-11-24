/*----------------------------------------------------------------------------
 *      Name:    SoftUart.c
 *      Purpose: Software UART on the CAN transceiver pins
 *
 *  PB8 (CAN_RX, remapped) is also TIM4_CH3, PB9 (CAN_TX) is TIM4_CH4.
 *  TIM4 runs free at 72 MHz:
 *    - channel 3 captures the falling edge of the start bit, so the edge
 *      time is latched by hardware and does not depend on interrupt latency
 *    - channel 1 compare then fires in the middle of every following bit
 *    - channel 4 (transmit build only) drives PB9 from the output compare,
 *      so every transmitted edge is placed by hardware as well
 *  In the receive-only build PB9 is a plain output held high, which keeps
 *  the transceiver recessive.
 *
 *  The interrupt handler never calls RTX functions. Received bytes go to a
 *  ring buffer that a task drains with SoftUart_Read(). Bytes to transmit
 *  are queued by SoftUart_Write() from the host USART interrupt.
 *
 *  All interrupts in this firmware share one preemption level
 *  (NVIC_PriorityGroup_0), so the TIM4 and USART2 handlers never interrupt
 *  each other. The transmit state relies on that.
 *---------------------------------------------------------------------------*/

#include "SoftUart.h"

/* TIM4 input clock: PCLK1 = 36 MHz, doubled because the APB1 prescaler is 2  */
#define SU_TIMER_CLOCK      72000000UL
#define SU_BIT_TICKS        ((u16)(SU_TIMER_CLOCK / SOFT_UART_BAUDRATE))
#define SU_HALF_BIT_TICKS   ((u16)(SU_BIT_TICKS / 2))

#if (SU_TIMER_CLOCK / SOFT_UART_BAUDRATE) > 0xFFFF
  #error "SOFT_UART_BAUDRATE is too low for a 16-bit timer without prescaler"
#endif

/* GPIOB: PB8 = transceiver RXD, PB9 = transceiver TXD                        */
#define SU_RX_PIN           ((u16)0x0100)
#define SU_TX_PIN           ((u16)0x0200)
#define SU_CRH_PB8_MASK     ((u32)0x0000000F)
#define SU_CRH_PB8_IPU      ((u32)0x00000008)    /* input with pull-up/down   */
#define SU_CRH_PB9_MASK     ((u32)0x000000F0)
#define SU_CRH_PB9_OUT_PP   ((u32)0x00000030)    /* push-pull output, 50 MHz  */
#define SU_CRH_PB9_AF_PP    ((u32)0x000000B0)    /* timer output, 50 MHz      */

#define SU_APB1ENR_TIM4EN   ((u32)0x00000004)
#define SU_APB2ENR_IOPBEN   ((u32)0x00000008)

/* TIM4 bits. CCx flags sit at the same position in SR, DIER and EGR.         */
#define SU_CC1              ((u16)0x0002)        /* compare: bit sampling     */
#define SU_CC3              ((u16)0x0008)        /* capture: start bit edge   */
#define SU_CC4              ((u16)0x0010)        /* compare: transmit edges   */
#define SU_SR_CC3OF         ((u16)0x0800)
#define SU_CCMR2_IC3_TI3    ((u16)0x0001)        /* CC3 is input, IC3 on TI3  */
#define SU_CCMR2_IC3_FILTER ((u16)0x0030)        /* 8 samples at 72 MHz       */
#define SU_CCMR2_OC4M_MASK  ((u16)0x7000)
#define SU_CCMR2_OC4_HIGH   ((u16)0x1000)        /* go high on next match     */
#define SU_CCMR2_OC4_LOW    ((u16)0x2000)        /* go low on next match      */
#define SU_CCMR2_OC4_IDLE   ((u16)0x5000)        /* forced high               */
#define SU_CCER_CC3E        ((u16)0x0100)
#define SU_CCER_CC3P        ((u16)0x0200)        /* falling edge              */
#define SU_CCER_CC4E        ((u16)0x1000)
#define SU_CR1_CEN          ((u16)0x0001)
#define SU_ARR_FREE_RUN     ((u16)0xFFFF)

#define SU_DATA_BITS        8
#define SU_BUFFER_SIZE      256                  /* u8 indexes wrap on their own */

volatile u32 SoftUart_FramingErrors = 0;
volatile u32 SoftUart_ParityErrors = 0;
volatile u32 SoftUart_OverrunErrors = 0;
volatile u32 SoftUart_TxOverrunErrors = 0;

static volatile u8 RxBuffer[SU_BUFFER_SIZE];
static volatile u8 RxHead = 0;                   /* written by the interrupt  */
static volatile u8 RxTail = 0;                   /* written by the reader     */

static u8 BitIndex;                              /* 0 = start bit             */
static u8 ShiftRegister;
#if SOFT_UART_PARITY != SOFT_UART_PARITY_NONE
static u8 OnesCount;
static u8 IsParityBad;
#endif

#if SOFT_UART_TX_Enable
static u8 TxBuffer[SU_BUFFER_SIZE];
static u8 TxHead = 0;
static u8 TxTail = 0;
static u16 TxShiftRegister;                      /* levels after the start bit */
static u8 TxBitsLeft;
static u8 IsTxActive = 0;
#endif

/*--------------------------- SoftUart_WaitStart ----------------------------
 *  Stop bit sampling and wait for the next start bit edge
 *---------------------------------------------------------------------------*/
static void SoftUart_WaitStart(void)
{
	(void)TIM4->CCR3;                            /* drop edges seen mid-frame */
	TIM4->SR = (u16)~(SU_CC3 | SU_SR_CC3OF);
	TIM4->DIER = (u16)((TIM4->DIER & SU_CC4) | SU_CC3);
}

/*--------------------------- SoftUart_StoreByte ----------------------------
 *  Put a received byte into the ring buffer
 *---------------------------------------------------------------------------*/
static void SoftUart_StoreByte(u8 data)
{
	u8 next = (u8)(RxHead + 1);

	if (next == RxTail)
	{
		SoftUart_OverrunErrors++;
		return;
	}

	RxBuffer[RxHead] = data;
	RxHead = next;
}

/*--------------------------- SoftUart_HandleStartEdge ----------------------
 *  Start bit edge captured: begin sampling half a bit after the edge
 *---------------------------------------------------------------------------*/
static void SoftUart_HandleStartEdge(void)
{
	u16 capture = TIM4->CCR3;                    /* reading clears the flag   */

	TIM4->CCR1 = (u16)(capture + SU_HALF_BIT_TICKS);
	TIM4->SR = (u16)~SU_CC1;
	TIM4->DIER = (u16)((TIM4->DIER & SU_CC4) | SU_CC1);
	BitIndex = 0;
	ShiftRegister = 0;
#if SOFT_UART_PARITY != SOFT_UART_PARITY_NONE
	OnesCount = 0;
	IsParityBad = 0;
#endif

	/* Entered so late that the compare point has already gone by             */
	if ((u16)(TIM4->CNT - capture) >= SU_HALF_BIT_TICKS)
		TIM4->EGR = SU_CC1;
}

/*--------------------------- SoftUart_HandleSample -------------------------
 *  Bit centre reached: read the line and advance the receive state
 *---------------------------------------------------------------------------*/
static void SoftUart_HandleSample(void)
{
	u8 level;

	TIM4->SR = (u16)~SU_CC1;
	level = (GPIOB->IDR & SU_RX_PIN) ? 1 : 0;

	if (BitIndex == 0)
	{
		if (level)                               /* glitch, not a start bit   */
		{
			SoftUart_WaitStart();
			return;
		}
	}
	else if (BitIndex <= SU_DATA_BITS)
	{
		ShiftRegister >>= 1;                     /* LSB first                 */
		if (level)
		{
			ShiftRegister |= 0x80;
#if SOFT_UART_PARITY != SOFT_UART_PARITY_NONE
			OnesCount++;
#endif
		}
	}
#if SOFT_UART_PARITY != SOFT_UART_PARITY_NONE
	else if (BitIndex == SU_DATA_BITS + 1)
	{
		/* Even parity: data ones + parity bit is even. Odd: it is odd.       */
		IsParityBad = (u8)((OnesCount + level + (SOFT_UART_PARITY == SOFT_UART_PARITY_ODD)) & 1);
	}
#endif
	else                                         /* stop bit                  */
	{
		if (!level)
			SoftUart_FramingErrors++;
#if SOFT_UART_PARITY != SOFT_UART_PARITY_NONE
		else if (IsParityBad)
			SoftUart_ParityErrors++;
#endif
		else
			SoftUart_StoreByte(ShiftRegister);

		SoftUart_WaitStart();
		return;
	}

	BitIndex++;
	TIM4->CCR1 = (u16)(TIM4->CCR1 + SU_BIT_TICKS);
}

#if SOFT_UART_TX_Enable
/*--------------------------- SoftUart_SetNextLevel -------------------------
 *  Choose the level PB9 takes at the next channel 4 match
 *---------------------------------------------------------------------------*/
static void SoftUart_SetNextLevel(u8 level)
{
	TIM4->CCMR2 = (u16)((TIM4->CCMR2 & ~SU_CCMR2_OC4M_MASK)
	            | (level ? SU_CCMR2_OC4_HIGH : SU_CCMR2_OC4_LOW));
}

/*--------------------------- SoftUart_LoadTxByte ---------------------------
 *  Take the next queued byte and build its data, parity and stop levels
 *
 *  Return:     0 if the queue is empty
 *---------------------------------------------------------------------------*/
static u8 SoftUart_LoadTxByte(void)
{
	u8 data;
#if SOFT_UART_PARITY != SOFT_UART_PARITY_NONE
	u8 ones = 0;
	u8 i;
#endif

	if (TxTail == TxHead)
		return 0;

	data = TxBuffer[TxTail];
	TxTail = (u8)(TxTail + 1);

#if SOFT_UART_PARITY != SOFT_UART_PARITY_NONE
	for (i = 0; i < SU_DATA_BITS; i++)
		ones = (u8)(ones + ((data >> i) & 1));

	/* data, parity bit, stop bit                                             */
	TxShiftRegister = (u16)(data
	                | (((ones + (SOFT_UART_PARITY == SOFT_UART_PARITY_ODD)) & 1) << SU_DATA_BITS)
	                | (1 << (SU_DATA_BITS + 1)));
	TxBitsLeft = SU_DATA_BITS + 2;
#else
	/* data, stop bit                                                         */
	TxShiftRegister = (u16)(data | (1 << SU_DATA_BITS));
	TxBitsLeft = SU_DATA_BITS + 1;
#endif

	return 1;
}

/*--------------------------- SoftUart_HandleTxEdge -------------------------
 *  A transmit edge has just been output: program the one a bit time later
 *---------------------------------------------------------------------------*/
static void SoftUart_HandleTxEdge(void)
{
	TIM4->SR = (u16)~SU_CC4;

	if (TxBitsLeft > 0)
	{
		SoftUart_SetNextLevel((u8)(TxShiftRegister & 1));
		TxShiftRegister >>= 1;
		TxBitsLeft--;
	}
	else if (SoftUart_LoadTxByte())
	{
		SoftUart_SetNextLevel(0);                /* start bit after one stop  */
	}
	else
	{
		/* Stop bit is on the line and nothing is queued                      */
		TIM4->DIER &= (u16)~SU_CC4;
		IsTxActive = 0;
		return;
	}

	TIM4->CCR4 = (u16)(TIM4->CCR4 + SU_BIT_TICKS);
}

/*--------------------------- SoftUart_Write --------------------------------
 *  Queue one byte for transmission on the bus.
 *  Call only from interrupt context (see the note at the top of this file).
 *
 *  Parameter:  data:       Byte to send
 *---------------------------------------------------------------------------*/
void SoftUart_Write(u8 data)
{
	u8 next = (u8)(TxHead + 1);

	if (next == TxTail)
	{
		SoftUart_TxOverrunErrors++;
		return;
	}

	TxBuffer[TxHead] = data;
	TxHead = next;

	if (IsTxActive)
		return;

	/* Start bit one bit time from now, so the previous stop bit is complete  */
	SoftUart_LoadTxByte();
	IsTxActive = 1;
	SoftUart_SetNextLevel(0);
	TIM4->CCR4 = (u16)(TIM4->CNT + SU_BIT_TICKS);
	TIM4->SR = (u16)~SU_CC4;
	TIM4->DIER |= SU_CC4;
}
#endif

/*--------------------------- SoftUart_Init ---------------------------------
 *  Take over the CAN pins and start listening. CAN must not be initialised.
 *  The host USART must already be initialised.
 *---------------------------------------------------------------------------*/
void SoftUart_Init(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC->APB2ENR |= SU_APB2ENR_IOPBEN;
	RCC->APB1ENR |= SU_APB1ENR_TIM4EN;

	/* Set both output latches first: PB9 comes up high, PB8 gets a pull-up   */
	GPIOB->BSRR = SU_TX_PIN | SU_RX_PIN;
	GPIOB->CRH = (GPIOB->CRH & ~(SU_CRH_PB8_MASK | SU_CRH_PB9_MASK))
	           | SU_CRH_PB8_IPU | SU_CRH_PB9_OUT_PP;

	TIM4->CR1 = 0;
	TIM4->DIER = 0;
	TIM4->PSC = 0;
	TIM4->ARR = SU_ARR_FREE_RUN;
	TIM4->CCMR1 = 0;                             /* CC1: compare, no output   */
#if SOFT_UART_TX_Enable
	TIM4->CCMR2 = SU_CCMR2_IC3_TI3 | SU_CCMR2_IC3_FILTER | SU_CCMR2_OC4_IDLE;
	TIM4->CCER = SU_CCER_CC3E | SU_CCER_CC3P | SU_CCER_CC4E;
#else
	TIM4->CCMR2 = SU_CCMR2_IC3_TI3 | SU_CCMR2_IC3_FILTER;
	TIM4->CCER = SU_CCER_CC3E | SU_CCER_CC3P;
#endif

	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	TIM4->CR1 = SU_CR1_CEN;
	SoftUart_WaitStart();

#if SOFT_UART_TX_Enable
	/* The timer output is already forced high, so the hand-over is clean     */
	GPIOB->CRH = (GPIOB->CRH & ~SU_CRH_PB9_MASK) | SU_CRH_PB9_AF_PP;

	/* Host bytes arrive in USART2_IRQHandler and go to SoftUart_Write        */
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
#endif
}

/*--------------------------- SoftUart_Read ---------------------------------
 *  Copy received bytes out of the ring buffer
 *
 *  Parameter:  buffer:     Destination
 *              size:       Space in destination
 *
 *  Return:     Number of bytes copied (0 if nothing is waiting)
 *---------------------------------------------------------------------------*/
u16 SoftUart_Read(u8 *buffer, u16 size)
{
	u16 count = 0;

	while (count < size && RxTail != RxHead)
	{
		buffer[count++] = RxBuffer[RxTail];
		RxTail = (u8)(RxTail + 1);
	}

	return count;
}

/*--------------------------- TIM4_IRQHandler -------------------------------
 *  Channel 3 capture: start bit edge. Channel 1 compare: bit centre.
 *  Channel 4 compare: transmit edge.
 *---------------------------------------------------------------------------*/
void TIM4_IRQHandler(void)
{
	u16 pending = (u16)(TIM4->SR & TIM4->DIER);

	if (pending & SU_CC3)
		SoftUart_HandleStartEdge();
	else if (pending & SU_CC1)
		SoftUart_HandleSample();

#if SOFT_UART_TX_Enable
	if (pending & SU_CC4)
		SoftUart_HandleTxEdge();
#endif
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
