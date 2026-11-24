#include <RTL.h>

#include "Tasks.h"
#include "RTX_CAN.h"
#include "CAN_Hw.h"
#include "USB2CAN_Func.h"
#include "USART.h"
#include "Flash.h"

extern bool USB2CAN_First;
extern u32 	USB2CAN_Baudrate;
extern u32 	USB2CAN_Mask;
extern u32 	USB2CAN_Mask_Id;
extern u32 	USB2CAN_FilterIndex;
extern bool USB2CAN_FilterEnable;
extern u32	USB2CAN_FlashCleaned;

static bool bDataFlag = FALSE;


/*--------------------------------
		Configure CAN according to the received messagesCAN
Thread 1: receives data from the USART; a configuration packet configures CAN, anything else is forwarded directly
Thread 2: CAN receive thread
---------------------------------*/
OS_TID task1, task2, task3;

__task void CAN_RecvTask(void)
{
	CAN_msg msg;
	while(1)
	{
		CAN_receive(1, &msg, 0xffff);
		USART_SendPackage(USART_2, -1, (u8 *)&msg, sizeof(msg) , 0xffff);
		bDataFlag = TRUE;
	}
}

u32 t = 0;
__task void CANConfigAndSendTask(void)
{
	CAN_msg configMsg;
	u8 buf[40];
	bool bIDAndMaskInFlash = FALSE;
	
	//Read the data stored in FLASH
	bIDAndMaskInFlash = CheckFLASH();
	while(1)
	{
		if(bIDAndMaskInFlash && USB2CAN_First)
		{
			CANInitFromFLASH();
		}
		else
		{
			if(USB2CAN_First)
			{
				CANInitFLASH();
			}
			
			if(USART_RecvPackage(USART_2, buf, 0xffff) != 0)
			{
				if(CheckCANMsgAvailable((CAN_msg*)buf))
				{
					memcpy((u8 *)&configMsg, buf, sizeof(configMsg));
					CAN_Operator(configMsg);
					t++;
					bDataFlag = TRUE;
				}
			}
		}
	}
}

__task void LightTask(void)
{
	while(1)
	{
		if(bDataFlag)
		{
			GPIO_SetBits(GPIOA, GPIO_Pin_0);
			os_dly_wait(100);
			GPIO_ResetBits(GPIOA, GPIO_Pin_0);			   
			os_dly_wait(100);
			
			bDataFlag = FALSE;
		}
	}
}

__task void InitTask (void) 
{  
	os_tsk_prio_self(100);

	GPIO_ResetBits(GPIOA, GPIO_Pin_0);

	//CANInitFLASH();

	//Initialize the serial port
	USARTInitFromFLASH(USART_2|NO_REMAP);
	
	//Configure CAN over USBCAN
	task3 = os_tsk_create(LightTask, 2);
	task2 = os_tsk_create(CAN_RecvTask, 4);
	task1 = os_tsk_create(CANConfigAndSendTask, 6);
	
	
	os_tsk_delete_self ();
}







