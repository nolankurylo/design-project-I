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

#define release_queue_length 100
#define TEST_BENCH 1

/*-----------------------------------------------------------*/


#define amber  	0
#define green  	1
#define red  	2
#define blue  	3

#define amber_led	LED3
#define green_led	LED4
#define red_led		LED5
#define blue_led	LED6
#define INCLUDE_vTaskDelete 1

static void prvSetupHardware( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
static void Monitor_Task( void *pvParameters );
static void Deadline_Driven_Task_Generator1( void *pvParameters );

//static void Deadline_Driven_Task_Generator2( void *pvParameters );
//static void Deadline_Driven_Task_Generator3( void *pvParameters );
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

struct dd_task_list {
    struct dd_task task;
    struct dd_task_list* next;
};


void listInsert(struct dd_task_list* head, struct dd_task new_task, int sort_flag);
void release_dd_task(TaskHandle_t, uint32_t, uint32_t, uint32_t, uint32_t);

void delete_dd_task(uint32_t);

void get_active_dd_task_list(void);

void get_complete_dd_task_list(void);

void get_overdue_dd_task_list(void);


xQueueHandle release_queue = 0;
xQueueHandle complete_queue = 0;



/*-----------------------------------------------------------*/

int main(void)
{

//	/* Initialize LEDs */
//	STM_EVAL_LEDInit(amber_led);
//	STM_EVAL_LEDInit(green_led);
//	STM_EVAL_LEDInit(red_led);
//	STM_EVAL_LEDInit(blue_led);

	/* Configure the system ready to run the demo.  The clock configuration
	can be done here if it was not done before main() was called. */
	prvSetupHardware();

	/* Create the queue used by the queue send and queue receive tasks.
	http://www.freertos.org/a00116.html */
	release_queue = xQueueCreate( 	release_queue_length,		/* The number of items the queue can hold. */
							sizeof( struct dd_task ) );	/* The size of each item the queue holds. */
	complete_queue = xQueueCreate( 	release_queue_length,		/* The number of items the queue can hold. */
								sizeof( uint32_t ) );	/* The size of each item the queue holds. */


	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry( release_queue, "ReleaseQueue" );
	vQueueAddToRegistry( complete_queue, "CompleteQueue" );


//	xTaskCreate( Monitor_Task, "Monitor", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
	xTaskCreate( Deadline_Driven_Task_Generator1, "Gen1", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
//	xTaskCreate( Deadline_Driven_Task_Generator2, "Gen2", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
//	xTaskCreate( Deadline_Driven_Task_Generator3, "Gen3", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
	xTaskCreate( Deadline_Driven_Scheduler, "Scheduler", configMINIMAL_STACK_SIZE, NULL, 3, NULL);



	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/
//	DD-tasks (dd_task) struct
//	DD-task lists (active/complete/overdue)
// release_dd_task
// complete_dd_task
// get_active_dd_task_list
// get_complete_dd_task_list
// get_overdue_dd_task_list
//
static void Deadline_Driven_Scheduler( void *pvParameters )
{

	/* linked lists for tasks */
	struct dd_task_list* activeHead = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list* completeHead = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list* overdueHead = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	activeHead->next = NULL;
	completeHead->next = NULL;
	overdueHead->next = NULL;

	struct dd_task new_release_task;
	uint32_t completed_task_id;
	while(1)
	{

		// Release new task if CPU available (no current DDS task running)
		if(activeHead->next != NULL && activeHead->next->task.release_time == -1){
			activeHead->next->task.release_time = (uint32_t)xTaskGetTickCount();
			xTaskCreate(User_Defined_Task,"UserDefinedTask",configMINIMAL_STACK_SIZE, &activeHead->next->task, 3, &(activeHead->next->task.t_handle));
		}
		// Receive new tasks from generator
		if(xQueueReceive(release_queue, &new_release_task, 0))
		{
			printf("received release task: %d\n", new_release_task.task_id);
			listInsert(activeHead, new_release_task, 1);
		}

		// Complete DDS tasks
		if(xQueueReceive(complete_queue, &completed_task_id, 0))
		{
			printf("completed task: %d\n", completed_task_id);
			printf("active head: %d\n", activeHead->next->task.task_id);
			vTaskPrioritySet(activeHead->next->task.t_handle, 1);
			printf("set\n");
			vTaskDelete(activeHead->next->task.t_handle);

			printf("Deleted\n");

//			completed_task = pop_from_ll
//			listInsert(completeHead, completed, 0);
		}

		// Modify Active list for overdue



		printf("DDS\n");
		vTaskDelay(pdMS_TO_TICKS(1));
	}

}

void listInsert(struct dd_task_list* head, struct dd_task new_task, int sort_flag)
{
	struct dd_task_list *new_node = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	new_node->task = new_task;
	new_node->next = NULL;
	struct dd_task_list *curr = head;
    if(sort_flag == 1)
    {
    	while(curr->next != NULL)
    	{
    		if(new_node->task.absolute_deadline < curr->next->task.absolute_deadline)
    		{
    			new_node->next = curr->next;
				curr->next = new_node;
				return;
    		}
    		else curr = curr->next;
    	}
    }
    else {
    	while(curr->next != NULL)
    	{
    		curr = curr->next;
    	}
    }
	curr->next = new_node;
}


/*-----------------------------------------------------------*/


void release_dd_task(TaskHandle_t t_handle, uint32_t type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline){

	struct dd_task new_dd_task;


	new_dd_task.type = type;
	new_dd_task.task_id = task_id;
	new_dd_task.execution_time = execution_time;
	new_dd_task.absolute_deadline = absolute_deadline;
	new_dd_task.release_time = -1;

	if(!xQueueSend(release_queue, &new_dd_task, 100)){
		printf("Failed to send new dd Task to release queue\n");
	}

}


/*-----------------------------------------------------------*/
static void User_Defined_Task( void *pvParameters )
{
	struct dd_task* curr_task = (struct dd_task*) pvParameters;


	int startTime = (int)xTaskGetTickCount();
	int endTime = startTime + (int) curr_task->execution_time;
	printf("Running User Defined Task %d: start: %d end: %d\n", curr_task->task_id, startTime, endTime);
	while ((int)xTaskGetTickCount() < endTime);

	printf("Done: %d\n" , (int)xTaskGetTickCount());
	uint32_t task_id = curr_task->task_id;

	if(!xQueueSend(complete_queue, &task_id, 0)){
		printf("Failed to send new dd Task to complete queue\n");
	}
	vTaskSuspend(NULL);



}
/*-----------------------------------------------------------*/

static void Monitor_Task( void *pvParameters )
{
	printf("MONITOR\n");
	vTaskDelay(1000);
}


static void Deadline_Driven_Task_Generator1( void *pvParameters )
{
	uint32_t i = 0;

	while (1){
		printf("releasing tasks from generator...\n");
		if (TEST_BENCH == 1){ // test bench 1


			release_dd_task(User_Defined_Task, 0, 1, 95, 500 + i*500); // 0 for periodic
//			release_dd_task(User_Defined_Task, 0, 2, 150, 500 + i*1500);
//			release_dd_task(User_Defined_Task, 0, 3, 250, 750 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(500));


		}
		else if (TEST_BENCH == 2){ // test bench 2
			release_dd_task(User_Defined_Task, 0, 1, 95, 250 + i*1500);
//			release_dd_task(0, 2, 150, 500 + i*1500);
//			release_dd_task(0, 3, 250, 750 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));

		}
		else { //Test Bench 3
			release_dd_task(User_Defined_Task, 0, 1, 100, 500 + i*500);
//			release_dd_task(0, 2, 200, 500 + i*500);
//			release_dd_task(0, 2, 200, 500 + i*500);
			vTaskDelay(pdMS_TO_TICKS(500));
		}
		i++;
	}
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
