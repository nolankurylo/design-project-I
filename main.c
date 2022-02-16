/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved
    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.
    This file is part of the FreeRTOS distribution.
    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.
    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************
    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html
    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************
    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wwrong?".  Have you
    defined configASSERT()?
    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.
    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.
    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.
    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.
    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.
    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.
    1 tab == 4 spaces!
*/

/*
FreeRTOS is a market leading RTOS from Real Time Engineers Ltd. that supports
31 architectures and receives 77500 downloads a year. It is professionally
developed, strictly quality controlled, robust, supported, and free to use in
commercial products without any requirement to expose your proprietary source
code.
This simple FreeRTOS demo does not make use of any IO ports, so will execute on
any Cortex-M3 of Cortex-M4 hardware.  Look for TODO markers in the code for
locations that may require tailoring to, for example, include a manufacturer
specific header file.
This is a starter project, so only a subset of the RTOS features are
demonstrated.  Ample source comments are provided, along with web links to
relevant pages on the http://www.FreeRTOS.org site.
Here is a description of the project's functionality:
The main() Function:
main() creates the tasks and software timers described in this section, before
starting the scheduler.
The Queue Send Task:
The queue send task is implemented by the prvQueueSendTask() function.
The task uses the FreeRTOS vTaskDelayUntil() and xQueueSend() API functions to
periodically send the number 100 on a queue.  The period is set to 200ms.  See
the comments in the function for more details.
http://www.freertos.org/vtaskdelayuntil.html
http://www.freertos.org/a00117.html
The Queue Receive Task:
The queue receive task is implemented by the prvQueueReceiveTask() function.
The task uses the FreeRTOS xQueueReceive() API function to receive values from
a queue.  The values received are those sent by the queue send task.  The queue
receive task increments the ulCountOfItemsReceivedOnQueue variable each time it
receives the value 100.  Therefore, as values are sent to the queue every 200ms,
the value of ulCountOfItemsReceivedOnQueue will increase by 5 every second.
http://www.freertos.org/a00118.html
An example software timer:
A software timer is created with an auto reloading period of 1000ms.  The
timer's callback function increments the ulCountOfTimerCallbackExecutions
variable each time it is called.  Therefore the value of
ulCountOfTimerCallbackExecutions will count seconds.
http://www.freertos.org/RTOS-software-timer.html
The FreeRTOS RTOS tick hook (or callback) function:
The tick hook function executes in the context of the FreeRTOS tick interrupt.
The function 'gives' a semaphore every 500th time it executes.  The semaphore
is used to synchronise with the event semaphore task, which is described next.
The event semaphore task:
The event semaphore task uses the FreeRTOS xSemaphoreTake() API function to
wait for the semaphore that is given by the RTOS tick hook function.  The task
increments the ulCountOfReceivedSemaphores variable each time the semaphore is
received.  As the semaphore is given every 500ms (assuming a tick frequency of
1KHz), the value of ulCountOfReceivedSemaphores will increase by 2 each second.
The idle hook (or callback) function:
The idle hook function queries the amount of free FreeRTOS heap space available.
See vApplicationIdleHook().
The malloc failed and stack overflow hook (or callback) functions:
These two hook functions are provided as examples, but do not contain any
functionality.
*/

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include "stm32f4_discovery.h"
/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_adc.h"



/*-----------------------------------------------------------*/
#define mainQUEUE_LENGTH 100

#define amber  	0
#define green  	1
#define red  	2
#define blue  	3

#define amber_led	LED3
#define green_led	LED4
#define red_led		LED5
#define blue_led	LED6

#define TRAFFIC_RED_LIGHT		GPIO_Pin_0
#define TRAFFIC_AMBER_LIGHT		GPIO_Pin_1
#define TRAFFIC_GREEN_LIGHT 	GPIO_Pin_2

#define SHIFT_REGISTER_RST GPIO_Pin_8
#define SHIFT_REGISTER_CLK GPIO_Pin_7
#define SHIFT_REGISTER_DATA GPIO_Pin_6

#define POT_INPUT GPIO_Pin_3
#define TRAFFIC_ARRAY_LEN 19

/*
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 * Init GPIO and ADC here
 */
static void hardwareInit( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
static void Manager_Task( void *pvParameters );
static void Traffic_Task( void *pvParameters );
void shiftClockPointer(void);
void moveTrafficRight(int cars_array[], int new_car);
void updateTraffic(int cars[]);
int updateFlow(int *flow_spaces, int spaces_to_add);

xQueueHandle xQueue_handle = 0;


/*-----------------------------------------------------------*/

int main(void)
{



	/* Configure the system ready to run the demo.  The clock configuration
	can be done here if it was not done before main() was called. */
	hardwareInit();


	/* Create the queue used by the queue send and queue receive tasks.
	http://www.freertos.org/a00116.html */
	xQueue_handle = xQueueCreate( 	mainQUEUE_LENGTH,		/* The number of items the queue can hold. */
							sizeof( uint16_t ) );	/* The size of each item the queue holds. */

	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry( xQueue_handle, "MainQueue" );

	xTaskCreate( Manager_Task, "Manager", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
	xTaskCreate(Traffic_Task, "Traffic", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/

static void Manager_Task( void *pvParameters )
{
	uint16_t next_car = 0;

	ADC_SoftwareStartConv(ADC1);


	int flow_spaces = 3; // keeps track of the current num spaces between newly added cars
	while(1)
	{

		while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
		int flow = ADC_GetConversionValue(ADC1);
		printf("%i\n", flow);

		if(flow < 1000){ // LOW
			next_car = updateFlow(&flow_spaces, 3);
		}
		else if((flow >= 1000) && (flow < 2000)){ //Med
			next_car = updateFlow(&flow_spaces, 2);
		}
		else if((flow >= 2000) && (flow < 3000)){ //Med-High
			next_car = updateFlow(&flow_spaces, 1);
		}
		else if(flow >= 3000){ //High
			next_car = updateFlow(&flow_spaces, 0);
		}


		printf("next car: %d\n", next_car);

		if( xQueueSend(xQueue_handle,&next_car,1000))
		{

			vTaskDelay(1000);
		}
		else
		{
			printf("Manager Failed!\n");
		}




	}
}
/*-----------------------------------------------------------*/

static void Traffic_Task( void *pvParameters ) // waits for new cars to be sent in the queue and updates the display
{
	int cars_array[TRAFFIC_ARRAY_LEN + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	uint16_t next_car;
	while(1)
	{
		if(xQueueReceive(xQueue_handle, &next_car, 500))
		{
			vTaskDelay(250);
			printf("adding new car: %d\n", next_car);
			moveTrafficRight(cars_array, next_car);
		}
		updateTraffic(cars_array);


	}
}

int updateFlow(int *flow_spaces, int spaces_to_add){ // current number of spaces between cars and the number of
	// spaces there needs to be between cars at this flow level
	uint16_t next_car;
	if(*flow_spaces < spaces_to_add){
		next_car = 0;
		*flow_spaces += 1;
	}
	else{
		next_car = 1;
		*flow_spaces = 0;
	}
	return next_car;

}

void moveTrafficRight(int cars_array[], int new_car){ // rotate array right and add new car to first index
	for(int i = TRAFFIC_ARRAY_LEN; i > 0 ; i--){
		cars_array[i]=cars_array[i-1];
	}
	cars_array[0] = new_car;
}

void updateTraffic(int cars[]){
	for(int i = 0; i <= TRAFFIC_ARRAY_LEN; i++){
		if (cars[TRAFFIC_ARRAY_LEN-i]){ // add car
			GPIO_SetBits(GPIOC, SHIFT_REGISTER_RST);
			GPIO_SetBits(GPIOC, SHIFT_REGISTER_DATA); // push in "on" car
			shiftClockPointer(); // shift right
		}
		else { // add no car
			GPIO_SetBits(GPIOC, SHIFT_REGISTER_RST);
			GPIO_ResetBits(GPIOC, SHIFT_REGISTER_DATA); // push in "off" car
			shiftClockPointer(); // shift right
		}
	}
}


void addZero(){

	 // turn light off
	shiftClockPointer();
	printf("move traffic\n");

}

void shiftClockPointer(){
	GPIO_SetBits(GPIOC, SHIFT_REGISTER_CLK);
	GPIO_ResetBits(GPIOC, SHIFT_REGISTER_CLK);
	GPIO_SetBits(GPIOC, SHIFT_REGISTER_CLK);
}

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.
	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software 
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.
	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}
/*-----------------------------------------------------------*/

static void hardwareInit( void )
{
	/* Ensure all priority bits are assigned as preemption priority bits.
	http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	NVIC_SetPriorityGrouping( 0 );



	// Enable clocks for GPIOC
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);


	// GPIO and SPC init

	GPIO_InitTypeDef TrafficLightGPIOStruct;

	TrafficLightGPIOStruct.GPIO_Mode = GPIO_Mode_OUT;
	TrafficLightGPIOStruct.GPIO_OType = GPIO_OType_PP;
	TrafficLightGPIOStruct.GPIO_Pin = TRAFFIC_RED_LIGHT | TRAFFIC_AMBER_LIGHT | TRAFFIC_GREEN_LIGHT | SHIFT_REGISTER_RST | SHIFT_REGISTER_CLK | SHIFT_REGISTER_DATA;
	TrafficLightGPIOStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	TrafficLightGPIOStruct.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOC, &TrafficLightGPIOStruct);

	// potentiometer GPIO setup. mode to anologue input

	GPIO_InitTypeDef PotGPIOStruct;

	PotGPIOStruct.GPIO_Mode = GPIO_Mode_AN;
	PotGPIOStruct.GPIO_OType = GPIO_OType_OD;
	PotGPIOStruct.GPIO_Pin = POT_INPUT;
	PotGPIOStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
//	PotGPIOStruct.GPIO_Speed = GPIO_Speed_2MHz;

	GPIO_Init(GPIOC, &PotGPIOStruct);


	// ADC setup
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	ADC_InitTypeDef ADCInitStruct;
	ADCInitStruct.ADC_Resolution = ADC_Resolution_12b;
	ADCInitStruct.ADC_ScanConvMode = DISABLE;
	ADCInitStruct.ADC_ContinuousConvMode = ENABLE;
	ADCInitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConvEdge_None;
	ADCInitStruct.ADC_DataAlign = ADC_DataAlign_Right;
	ADCInitStruct.ADC_ExternalTrigConvEdge=ADC_ExternalTrigConvEdge_None;

	ADC_Init(ADC1, &ADCInitStruct);
	ADC_Cmd(ADC1, ENABLE);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 1, ADC_SampleTime_144Cycles);



}



//static void Traffic_Task( void *pvParameters )
//{
//	int traffic_array[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//	int count = 0;
//	uint16_t rx_data;
//	while(1)
//	{
//		if(xQueueReceive(xQueue_handle, &rx_data, 500))
//		{
//			if(rx_data == "RED")
//			{
//				vTaskDelay(250);
//				print("RED ON\n");
//				GPIO_ResetBits(GPIOC, TRAFFIC_RED_LIGHT);
//
//			}
//			else
//			{
//				if( xQueueSend(xQueue_handle,&rx_data,1000))
//					{
//
//						vTaskDelay(500);
//					}
//			}
//		}
//		updateTraffic();
//
//	}
//}
