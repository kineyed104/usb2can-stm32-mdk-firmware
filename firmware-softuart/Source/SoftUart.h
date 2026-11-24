/*----------------------------------------------------------------------------
 *      Name:    SoftUart.h
 *      Purpose: Receive-only software UART on the CAN transceiver RXD pin
 *               (PB8 = TIM4_CH3). Used when the field bus carries UART frames
 *               over a CAN physical layer.
 *---------------------------------------------------------------------------*/

#ifndef __SOFT_UART_H
#define __SOFT_UART_H

#include "stm32f10x_lib.h"

/* 1: listen for UART on the CAN pins and forward raw bytes to the host.
      The CAN controller is never started, so nothing is driven on the bus.
   0: original USB2CAN behaviour.                                            */
#define SOFT_UART_Enable        1

/* 1: bytes sent by the host are transmitted on the bus (bench testing).
      Everything transmitted is also received back, because the transceiver
      reports the bus state on RXD.
   0: receive only, the bus is never driven. USE THIS ON A LIVE FIELD BUS.   */
#define SOFT_UART_TX_Enable     0

/* Line settings of the field bus                                            */
#define SOFT_UART_BAUDRATE      9600

#define SOFT_UART_PARITY_NONE   0
#define SOFT_UART_PARITY_EVEN   1
#define SOFT_UART_PARITY_ODD    2
#define SOFT_UART_PARITY        SOFT_UART_PARITY_NONE

/* Largest block handed to USART_SendBuf in one call (MAX_PACK_SIZE - 2)     */
#define SOFT_UART_CHUNK_SIZE    30

/* Diagnostic counters, readable from the debugger                           */
extern volatile u32 SoftUart_FramingErrors;
extern volatile u32 SoftUart_ParityErrors;
extern volatile u32 SoftUart_OverrunErrors;
extern volatile u32 SoftUart_TxOverrunErrors;

void SoftUart_Init(void);
u16  SoftUart_Read(u8 *buffer, u16 size);
#if SOFT_UART_TX_Enable
void SoftUart_Write(u8 data);
#endif

#endif
