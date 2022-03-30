/* Traffic Light System (TLS) Project Code
 * Authors: Nolan Kurylo, Treavor Gagne
 * ECE 455 B01
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

#define amber_led	LED3
#define green_led	LED4
#define red_led		LED5

#define GREEN_STATE	0
#define AMBER_STATE	1
#define RED_STATE	2

#define TRAFFIC_RED_LIGHT		GPIO_Pin_0
#define TRAFFIC_AMBER_LIGHT		GPIO_Pin_1
#define TRAFFIC_GREEN_LIGHT 	GPIO_Pin_2

#define SHIFT_REGISTER_RST GPIO_Pin_8
#define SHIFT_REGISTER_CLK GPIO_Pin_7
#define SHIFT_REGISTER_DATA GPIO_Pin_6

#define POT_INPUT GPIO_Pin_3
#define TRAFFIC_ARRAY_LEN 19

static void hardwareInit( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
static void Manager_Task( void );
static void Traffic_Task( void );
static void Traffic_Light_State_Task( void );
void shiftClockPointer( void );
void moveTrafficRight( int, int, int );
void updateTraffic( int );
int updateFlow( int );
void vCallbackFunction( TimerHandle_t );

xQueueHandle xQueue_nextCar = 0;
xQueueHandle xQueue_flowRate = 0;
xQueueHandle xQueue_lightState = 0;
xQueueHandle xQueue_updatedLight = 0;
TimerHandle_t xTimer;

/*-----------------------------------------------------------*/

int main(void)
{
	/* Configure the GPIO, SPC, and Potentiometer */
	hardwareInit();

	/* Timer setup */
	xTimer = xTimerCreate("Traffic Timer", pdMS_TO_TICKS(1000), pdFALSE, (void *)0, vCallbackFunction);

	/* Queue for passing the next generated car (0 or 1) */
	xQueue_nextCar = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint16_t));
	/* Queue for passing the flow rate from the potentiometer */
	xQueue_flowRate = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint16_t));
	/* Queue for passing the current state of the light (GREEN, AMBER, RED) */
	xQueue_lightState = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint16_t));
	/* Queue for passing the light that needs to change to the timer Callback */
	xQueue_updatedLight = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint16_t));

	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry( xQueue_nextCar, "nextCarQueue" );
	vQueueAddToRegistry( xQueue_flowRate, "flowRateQueue" );
	vQueueAddToRegistry( xQueue_lightState, "lightStateQueue" );
	vQueueAddToRegistry( xQueue_updatedLight, "updatedLightQueue" );

	xTaskCreate(Manager_Task, "ManagerTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
	xTaskCreate(Traffic_Task, "TrafficTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
	xTaskCreate(Traffic_Light_State_Task, "LightStateTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	/* Set the initial state of the light to GREEN */
	int nextLight = GREEN_STATE;
	if( !xQueueSend(xQueue_updatedLight,&nextLight,500))
	{
		printf("Error sending Initial light state\n");
	}

	/* Start the tasks and get the timer running. */
	vTaskStartScheduler();
	return 0;
}

/*--------------------------- TASKS -------------------------------------*/

/* Main Task (highest priority) to receive potentiometer input, generate the next car, and
 * send the traffic flow rate + next car into the queues */
static void Manager_Task( void *pvParameters )
{
	uint16_t next_car = 0;

	ADC_SoftwareStartConv(ADC1);

	GPIO_ResetBits(GPIOC, TRAFFIC_RED_LIGHT);
	GPIO_ResetBits(GPIOC, TRAFFIC_AMBER_LIGHT);
	GPIO_SetBits(GPIOC, TRAFFIC_GREEN_LIGHT);

	int flow_rate; /* Corresponding rate of traffic flow based on corresponding probability */
	while(1)
	{

		while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
		int flow = ADC_GetConversionValue(ADC1);

		if(flow < 820){ // LOW
			next_car = updateFlow(5); // (1/5) 20% chance to add traffic
			flow_rate = 1;
		}
		else if((flow >= 820) && (flow < 1640)){ //LOW - MED
			next_car = updateFlow(4); // (1/4) 25% chance to add traffic
			flow_rate = 2;
		}
		else if((flow >= 1640) && (flow < 2460)){ // MED
			next_car = updateFlow(3); // (1/3) 33% chance to add traffic
			flow_rate = 3;
		}
		else if((flow >= 2460) && (flow < 3280)){ //MED - HIGH
			next_car = updateFlow(2); // (1/2) 50% chance to add traffic
			flow_rate = 4;
		}
		else{ // (flow >= 3280) //High
			next_car = updateFlow(1); // (1/1) 100% of adding traffic
			flow_rate = 5;
		}

		/* Send current traffic flow rate and the next car */
		if( xQueueSend(xQueue_flowRate, &flow_rate,1000) && xQueueSend(xQueue_nextCar,&next_car,1000))
		{
			vTaskDelay(750);
		}
		else
		{
			printf("ManagerTask Failed!\n");
		}
	}
}

/* Traffic Light State Task - determines timing of traffic lights based on received flow rate */
static void Traffic_Light_State_Task( void *pvParameters )
{

	uint16_t flow_rate;
	uint16_t curr_light;

	while(1)
	{
		if (xQueueReceive(xQueue_flowRate, &flow_rate, 100)){
			if(xQueueReceive(xQueue_updatedLight, &curr_light, 250))
			{

				/* Determine length of time for each light state based on the current flow rate */
				int timer_amount;
				float base = 1000;
				float offset = 2000;
				float inverse = (float) (1.0 / (float) flow_rate);

				int GREEN_TIME = flow_rate * base + offset;
				int AMBER_TIME = 2500;
				int RED_TIME =  inverse * base * 5 + offset;

				/* Change timer duration for the next time the light changes (timer ends) */
				if(curr_light == AMBER_STATE) timer_amount = AMBER_TIME;
				else if(curr_light == GREEN_STATE) timer_amount = GREEN_TIME;
				else if(curr_light == RED_STATE) timer_amount = RED_TIME;

				if( xTimerChangePeriod( xTimer, pdMS_TO_TICKS(timer_amount), 100 ) != pdPASS ) {
					printf("Failed to start timer\n");
				}

				/* Send the current light state to the timer callback so that it can change it when the timer is up */
				if( !xQueueSend(xQueue_lightState, &curr_light,1000) )
				{
					printf("Failed to send lightToUpdate to timer\n");
				}
			}

		}
		vTaskDelay(250);
	}
}

/* Traffic Task - waits to receive newly generated car and also updates traffic display by shifting the traffic */
static void Traffic_Task( void *pvParameters ) //
{
	int cars_array[TRAFFIC_ARRAY_LEN + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint16_t next_car;
	uint16_t current_light;

	while(1)
	{
		if( xQueueReceive(xQueue_nextCar, &next_car, 250) && xQueuePeek(xQueue_lightState, &current_light, 250)) // get next car from queue push by manager
		{
			moveTrafficRight(cars_array, next_car, current_light);
		}
		updateTraffic(cars_array); // update display
		vTaskDelay(100);
	}
}

/* Traffic Timer Callback function - receives the current light state, updates it to the next light state (and the display) and sends it back */
void vCallbackFunction( TimerHandle_t xTimer )
{
	uint16_t light_to_update;
	uint16_t next_light;

	if( xQueueReceive(xQueue_lightState, &light_to_update, 500) )
	{
		/* GREEN -> AMBER -> RED -> GREEN -> AMBER -> ... */
		next_light = (light_to_update + 1) % 3;

		/* Update Traffic Lights on display via GPIOC*/
		if(next_light == RED_STATE){
			GPIO_ResetBits(GPIOC, TRAFFIC_GREEN_LIGHT);
			GPIO_ResetBits(GPIOC, TRAFFIC_AMBER_LIGHT);
			GPIO_SetBits(GPIOC, TRAFFIC_RED_LIGHT);
		}
		else if(next_light == AMBER_STATE){
			GPIO_ResetBits(GPIOC, TRAFFIC_RED_LIGHT);
			GPIO_SetBits(GPIOC, TRAFFIC_AMBER_LIGHT);
			GPIO_ResetBits(GPIOC, TRAFFIC_GREEN_LIGHT);
		}
		else{ // GREEN STATE
			GPIO_ResetBits(GPIOC, TRAFFIC_RED_LIGHT);
			GPIO_ResetBits(GPIOC, TRAFFIC_AMBER_LIGHT);
			GPIO_SetBits(GPIOC, TRAFFIC_GREEN_LIGHT);
		}

		/* Send back the newly updated light state */
		if( !xQueueSend(xQueue_updatedLight, &next_light,1000) )
		{
			printf("Failed to send next_light to xQueue_lightState\n");
		}
	}
}

/*------------------------------------- FUNCTIONS -------------------------------------------*/
/* Generates a car (1) or no car (0) based on the input probability */
int updateFlow(int probability){
	uint16_t next_car;
	if(probability == 1) next_car = 1;
	else{
		int rate = rand() % (probability);
		if(rate == 0) next_car = 1;
		else next_car = 0;
	}
	return next_car;
}

/* Shifts traffic in the cars array to the right (based on the light state), adding in the newly generated car at the front of the array */
void moveTrafficRight(int cars_array[], int new_car, int current_state){
	if(current_state != GREEN_STATE){ // shifting the same for RED and AMBER light states
		for(int i = 7; i > 0 ; i--){ // shift traffic before the intersection, stopping at the intersection
			if(cars_array[i] == 0){
				cars_array[i] = cars_array[i-1];
				cars_array[i-1] = 0;
			}
		}
		if(cars_array[0] == 0) cars_array[0] = new_car; // add new car to front if there is room for it
		for(int i = TRAFFIC_ARRAY_LEN; i > 7 ; i--){ // shift traffic in and after the intersection to the right
			if(i == 8){
				cars_array[i] = 0;
				continue;
			}
			cars_array[i]=cars_array[i-1];
		}
	}
	else{ // GREEN light state, move all traffic thru
		for(int i = TRAFFIC_ARRAY_LEN; i > 0 ; i--){
			cars_array[i] = cars_array[i-1];
			cars_array[i-1] = 0;
		}
		cars_array[0] = new_car; // add new car to the front of array
	}
}

/* Updates the TLS display for the cars via SPC */
void updateTraffic(int cars[]){
	for(int i = 0; i <= TRAFFIC_ARRAY_LEN; i++){
		// reset shift register
		GPIO_SetBits(GPIOC, SHIFT_REGISTER_RST);

		// if car in position set bit on, else set bit off
		if (cars[TRAFFIC_ARRAY_LEN-i]) GPIO_SetBits(GPIOC, SHIFT_REGISTER_DATA);
		else GPIO_ResetBits(GPIOC, SHIFT_REGISTER_DATA);

		// move shift register to next position
		GPIO_ResetBits(GPIOC, SHIFT_REGISTER_CLK);
		GPIO_SetBits(GPIOC, SHIFT_REGISTER_CLK);
		GPIO_ResetBits(GPIOC, SHIFT_REGISTER_CLK);
	}
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
	/* Ensure all priority bits are assigned as preemption priority bits. */
	NVIC_SetPriorityGrouping( 0 );

	/* Enable clocks for GPIOC */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	/* GPIO and SPC init */
	GPIO_InitTypeDef TrafficLightGPIOStruct;
	TrafficLightGPIOStruct.GPIO_Mode = GPIO_Mode_OUT;
	TrafficLightGPIOStruct.GPIO_OType = GPIO_OType_PP;
	TrafficLightGPIOStruct.GPIO_Pin = TRAFFIC_RED_LIGHT | TRAFFIC_AMBER_LIGHT | TRAFFIC_GREEN_LIGHT | SHIFT_REGISTER_RST | SHIFT_REGISTER_CLK | SHIFT_REGISTER_DATA;
	TrafficLightGPIOStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	TrafficLightGPIOStruct.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOC, &TrafficLightGPIOStruct);

	/* Potentiometer setup via GPIO */
	GPIO_InitTypeDef PotGPIOStruct;

	PotGPIOStruct.GPIO_Mode = GPIO_Mode_AN;
	PotGPIOStruct.GPIO_OType = GPIO_OType_OD;
	PotGPIOStruct.GPIO_Pin = POT_INPUT;
	PotGPIOStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIOC, &PotGPIOStruct);

	/* Enable clock for ADC */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* ADC setup */
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