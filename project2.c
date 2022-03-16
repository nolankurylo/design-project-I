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

#define release_queue_length 100

/*-----------------------------------------------------------*/


#define amber  	0
#define green  	1
#define red  	2
#define blue  	3

#define amber_led	LED3
#define green_led	LED4
#define red_led		LED5
#define blue_led	LED6




/*
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 */
static void prvSetupHardware( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
static void Monitor_Task( void *pvParameters );
static void Deadline_Driven_Task_Generator( void *pvParameters );
static void Deadline_Driven_Scheduler( void *pvParameters );
static void User_Defined_Task( void *pvParameters );


struct dd_task {
	TaskHandle_t t_handle;
	uint32_t type; // PERIODIC or APERIODIC
	uint32_t task_id;
	uint32_t release_time;
	uint32_t execution_time;
	uint32_t absolute_deadline;
	uint32_t completion_time;
};

struct dd_task_lisk {
    struct dd_task task;
    struct dd_task_lisk* next;
};



void release_dd_task(TaskHandle_t, uint32_t, uint32_t, uint32_t, uint32_t);

void delete_dd_task(uint32_t);

void get_active_dd_task_list(void);

void get_complete_dd_task_list(void);

void get_overdue_dd_task_list(void);


xQueueHandle release_queue = 0;
xQueueHandle periodic_queue = 0;
TimerHandle_t xTimer;

/*-----------------------------------------------------------*/

int main(void)
{

	/* linked lists for tasks */
	struct dd_task_lisk* activeHead = NULL;
	struct dd_task_lisk* completeHead = NULL;
	struct dd_task_lisk* overdueHead = NULL;

	/* Initialize LEDs */
	STM_EVAL_LEDInit(amber_led);
	STM_EVAL_LEDInit(green_led);
	STM_EVAL_LEDInit(red_led);
	STM_EVAL_LEDInit(blue_led);

	/* Configure the system ready to run the demo.  The clock configuration
	can be done here if it was not done before main() was called. */
	prvSetupHardware();

	/* Create the queue used by the queue send and queue receive tasks.
	http://www.freertos.org/a00116.html */
	release_queue = xQueueCreate( 	release_queue_length,		/* The number of items the queue can hold. */
							sizeof( struct dd_task ) );	/* The size of each item the queue holds. */
	
	periodic_queue = xQueueCreate( 	release_queue_length,		/* The number of items the queue can hold. */
							sizeof( struct dd_task ) );	/* The size of each item the queue holds. */

	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry( release_queue, "ReleaseQueue" );
	vQueueAddToRegistry( periodic_queue, "ReleaseQueue" );

	xTaskCreate( Monitor_Task, "Monitor", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
	xTaskCreate( Deadline_Driven_Task_Generator, "Generator", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
	xTaskCreate( Deadline_Driven_Scheduler, "Scheduler", configMINIMAL_STACK_SIZE, NULL, 3, NULL);


	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/

static void Deadline_Driven_Scheduler( void *pvParameters )
{

//	DD-tasks (dd_task) struct
//	DD-task lists (active/complete/overdue)
// release_dd_task
// complete_dd_task
// get_active_dd_task_list
// get_complete_dd_task_list
// get_overdue_dd_task_list


	struct dd_task new_release_task;
	while(1)
	{
		if(xQueueReceive(release_queue, &new_release_task, 100))
		{
			printf("received release task: %d\n", new_release_task.task_id);

		}
		vTaskDelay(500);
	}

}


/*-----------------------------------------------------------*/

static void Deadline_Driven_Task_Generator( void *pvParameters )
{
//	generates dd-tasks periodically
// call from software timer callback, o.w. suspended
// does all the setup for the new task
// calls release_dd_task(task_id, release_type, absolute_time, completion)

	uint32_t i = 0;
	int bench = 1;
	while (1){
		if (bench == 1){ // test bench 1
			printf("releasing tasks from generator...\n");
			
			release_dd_task(User_Defined_Task, 0, 1, 95, 500 + i*1500); // 0 for periodic
			xTimer = xTimerCreate("Timer1", pdMS_TO_TICKS(500), pdTrue, (void *)0, vCallbackFunction);
			if(!xQueueSend(release_queue, &new_dd_task, 100)){
				printf("Failed to send new dd Task to release queue\n");
			}
			
			release_dd_task(User_Defined_Task, 0, 2, 150, 500 + i*1500);
			release_dd_task(User_Defined_Task, 0, 3, 250, 750 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(500));
			bench = 10;

//			release_dd_task(0, 1, 95, 1000 + i*1500);
//			release_dd_task(0, 2, 150, 1000 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(250));
//
//			release_dd_task(0, 3, 250, 1500 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(250));
//
//			release_dd_task(0, 1, 95, 1500 + i*1500);
//			release_dd_task(0, 2, 150, 1500 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(500));
		}
//		else if (bench == 2){ // test bench 2
//			release_dd_task(0, 1, 95, 250 + i*1500);
//			release_dd_task(0, 2, 150, 500 + i*1500);
//			release_dd_task(0, 3, 250, 750 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(250));
//
//			release_dd_task(0, 1, 95, 500 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(250));
//
//			release_dd_task(0, 1, 95, 750 + i*1500);
//			release_dd_task(0, 2, 150, 1000 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(250));
//
//			release_dd_task(0, 1, 95, 1000 + i*1500);
//			release_dd_task(0, 3, 250, 1500 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(250));
//
//			release_dd_task(0, 1, 95, 1250 + i*1500);
//			release_dd_task(0, 2, 150, 1500 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(250));
//
//			release_dd_task(0, 1, 95, 1500 + i*1500);
//			vTaskDelay(pdMS_TO_TICKS(250));
//		}
//		else { //Test Bench 3
//			release_dd_task(0, 1, 100, 500 + i*500);
//			release_dd_task(0, 2, 200, 500 + i*500);
//			release_dd_task(0, 2, 200, 500 + i*500);
//			vTaskDelay(pdMS_TO_TICKS(500));
//		}
		i++;
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

/*-----------------------------------------------------------*/
static void User_Defined_Task( void *pvParameters )
{
// report # of active dd-tasks
// report # of completed dd-tasks
// report # of overdue dd-tasks

// call functions  get_active_dd_task_list,
//	get_complete_dd_task_list,  and  get_overdue_dd_task_list



	while(1){
		printf("user defined task vunderunf\n");
		vTaskDelay(500);
	}
}
/*-----------------------------------------------------------*/

static void Monitor_Task( void *pvParameters )
{
// report # of active dd-tasks
// report # of completed dd-tasks
// report # of overdue dd-tasks

// call functions  get_active_dd_task_list,
//	get_complete_dd_task_list,  and  get_overdue_dd_task_list



//	uint16_t tx_data = amber;
//	while(1)
//	{
//
//
//		if( xQueueSend(xQueue_handle,&tx_data,1000))
//		{
//			printf("Manager: %u ON!\n", tx_data);
//			if(++tx_data == 4)
//				tx_data = 0;
//			vTaskDelay(1000);
//		}
//		else
//		{
//			printf("Manager Failed!\n");
//		}
//	}
	vTaskDelay(1000);
}



void release_dd_task(TaskHandle_t t_handle, uint32_t type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline){

	struct dd_task new_dd_task;

	new_dd_task.t_handle = t_handle;
	new_dd_task.type = type;
	new_dd_task.task_id = task_id;
	new_dd_task.execution_time = execution_time;
	new_dd_task.absolute_deadline = absolute_deadline;

	if(!xQueueSend(release_queue, &new_dd_task, 100)){
		printf("Failed to send new dd Task to release queue\n");
	}

}




void delete_dd_task(uint32_t task_id){

}




void get_active_dd_task_list(void){

}




void get_complete_dd_task_list(void){

}



void get_overdue_dd_task_list(void){

}












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

static void prvSetupHardware( void )
{
	/* Ensure all priority bits are assigned as preemption priority bits.
	http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	NVIC_SetPriorityGrouping( 0 );

	/* TODO: Setup the clocks, etc. here, if they were not configured before
	main() was called. */
}

