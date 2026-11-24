#ifndef USB2CAN_FUNC_H
#define USB2CAN_FUNC_H

#include <stm32f10x_lib.h>
#include "RTX_CAN.h"

/*---------------Memory layout--------------
			----------------------
 0x08008000:|    start marker    |
			----------------------
			|                    |
			----------------------
			|                    |
			----------------------
--------------------------------------*/

/*-------------------Definition-------------------*/
/*   Initial values   */
#define CAN_INIT_ENA		0x02
#define CAN_INIT_MASK		0x1fffffff
#define CAN_INIT_MASK_ID	0x1fffffff
#define CAN_INIT_BAUD		500000
#define CAN_INIT_INDEX		0
#define	CAN_INIT_ABOM		FALSE
#define CAN_INIT_ART		FALSE
#define CAN_INIT_VER		2

/* CHANNEL */
#define CAN_CONFIG_CHANNEL	0xff					//CAN configuration uses channel 0xff

/*  ID  */
#define CAN_CPU_INFO0_ID		0x01fffff0				//ID: CPU info, first two words
#define CAN_CPU_INFO1_ID		(CAN_CPU_INFO0_ID + 1)	//ID: CPU info, last word
#define CAN_VERSION_ID			0x01ffffe0				//ID: get version info
#define CAN_INIT_ID				0x01fffeff				//ID: CAN initialized successfully
#define CAN_FILTER_BASE_ID		0x01fffee0				//ID: get FILTER info (base ID)
#define CAN_BAUDRATE_ID 		0x01fffed0				//ID: get baud rate
#define CAN_RESET_ID			0x01fffec0				//ID: restart CAN
#define CAN_ABOM_ID				0x01fffeb0				//ID: set ABOM (automatic bus-off management)
#define CAN_ART_ID				0x01fffea0				//ID: set ART (automatic retransmission)
#define USART_BAUDRATE_ID		0x01fffe90				//ID: set USART baud rate			

/* FILTER related macros */
#define CAN_FILTER_IDX_MASK	0x0000000f				//extracts from the FILTER ID the INDEX
#define CAN_FILTER_EN		0x80000000				//FILTER enable flag
#define CAN_FILTER_DIS		0						//FILTER disable flag
#define CAN_FORMAT_EXT		0x40000000				//FORMAT is EXTENDED
#define CAN_FORMAT_STD		0						//FORMAT is STANDARD

/* FLASH related macros */
#define CAN_PARM_START		0xaaaaaaaa				//FLASH marker: data has already been stored
#define StartAddr 	 		((vu32)0x08008000)		//FLASH start address
#define EndAddr    			((vu32)0x0800C000)		//FLASH end address
#define CAN_PARM_NUM		48						//FLASH: number of u32 values stored
#define CAN_ADDR_START		0						//START is stored in the first array element
#define CAN_ADDR_BAUD		1						//the baud rate is stored in the second array element
#define CAN_ADDR_FILTER		2  //-43  42 in total		//FILTER entries are stored in array elements 2 to 42
#define CAN_ADDR_ABOM		44						//ABOM configuration position
#define CAN_ADDR_ART		45						//ART position
#define CAN_ADDR_VER		46						//VERSION position
#define USART_ADDR_BAUD		47



/*---------------Global Function--------------*/

//Functions used for initialization and reinitialization
bool CheckFLASH(void);
bool CANInitFLASH(void);
bool CANInitFromUSART(u32 *parm, u32 size, CAN_msg msg);
bool CANInitFromFLASH(void);
void USARTInitFromFLASH(u8 ctrl_remap);
void CAN_Operator(CAN_msg configMsg);

//Get and set FILTER
bool ReturnFilterInfo(u32 msgID);
bool SetFilterInfo(CAN_msg msg);

//Get and set the baud rate
bool ReturnBaudrate(void);
bool SetBaudrate(CAN_msg msg);

//Get and set ART
bool ReturnART(void);
bool SetART(CAN_msg msg);

//Get and set ABOM
bool ReturnABOM(void);
bool SetABOM(CAN_msg msg);

//Get the CPU information
bool ReturnCPU(void);

//Get the version information
bool ReturnVersion(void);

//Restart CAN
bool RemoteResetCan(CAN_msg msg);

//Erase FLASH
bool CleanFlash(CAN_msg msg);

#endif

