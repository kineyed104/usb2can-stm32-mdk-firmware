
#ifndef __SENSOR_CONTROL_BSL_H__
#define __SENSOR_CONTROL_BSL_H__

#include "stm32f10x_lib.h"
#include <RTL.h>
#include <stdint.h>

void RCC_Configuration(void);
       
void NVIC_Configuration(void);
     
void GPIO_Configuration(void);

void Timer3_Configuration(void);

void ADC_Configuration(void); 

void DMA_Configuration(void);  

uint64_t GetSystemTick(void);

void os_SetBeep(u8 n);

#define LIMIT(x, m) do{if((x) > (m)) x = (m); else if((x) < -(m)) x = -(m);}while(0)

#endif 
