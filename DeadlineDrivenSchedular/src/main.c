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

#define release_queue_length 20
#define TEST_BENCH 3

/*-----------------------------------------------------------*/


#define INCLUDE_vTaskDelete 1

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

struct dd_task_list {
    struct dd_task task;
    struct dd_task_list* next;
};

void listInsert(struct dd_task_list*, struct dd_task, int);
void release_dd_task(uint32_t, uint32_t, uint32_t, uint32_t);
void complete_dd_task(uint32_t);
void dd_create(struct dd_task_list* );
struct dd_task dd_delete(struct dd_task_list* , uint32_t);
void dd_remove_overdue(struct dd_task_list*, struct dd_task_list*);
int get_active_dd_task_list(struct dd_task_list*);
int get_complete_dd_task_list(struct dd_task_list*);
int get_overdue_dd_task_list(struct dd_task_list*);

xQueueHandle release_queue = 0;
xQueueHandle completed_queue = 0;
xQueueHandle active_list_queue = 0;
xQueueHandle overdue_list_queue = 0;
xQueueHandle complete_list_queue = 0;

/*-----------------------------------------------------------*/

int main(void)
{

	/* Configure the system ready */
	prvSetupHardware();
	printf("TEST BENCH: %d\n", TEST_BENCH);

	/* Create the queue used by the queue send and queue receive tasks. */
	release_queue = xQueueCreate( 	release_queue_length,		/* The items the queue can hold. */
							sizeof( struct dd_task ) );	/* The size of each item the queue holds. */
	completed_queue = xQueueCreate( 	release_queue_length,		/* The items the queue can hold. */
								sizeof( uint32_t ) );	/* The size of each item the queue holds. */
	active_list_queue = xQueueCreate( 1, sizeof( struct dd_task_list ) );
	overdue_list_queue = xQueueCreate( 1, sizeof( struct dd_task_list ) );
	complete_list_queue = xQueueCreate( 1, sizeof( struct dd_task_list ) );


	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry( release_queue, "ReleaseQueue" );
	vQueueAddToRegistry( completed_queue, "CompleteQueue" );
	vQueueAddToRegistry( active_list_queue, "ActiveListQueue" );
	vQueueAddToRegistry( overdue_list_queue, "OverdueListQueue" );
	vQueueAddToRegistry( complete_list_queue, "CompleteListQueue" );


	/* Create tasks*/
	xTaskCreate( Monitor_Task, "Monitor", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
	xTaskCreate( Deadline_Driven_Task_Generator, "Gen", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
	xTaskCreate( Deadline_Driven_Scheduler, "Scheduler", configMINIMAL_STACK_SIZE, NULL, 3, NULL);


	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	return 0;
}



/*--------------------------- TASKS -------------------------------------*/

/* Monitor Task (highest priority) to print the contents of each dd task list at the end of the hyperperiod */

static void Monitor_Task( void *pvParameters ){

	struct dd_task_list* overdueHead = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list* completeHead = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list* activeHead = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	int count = 0;

	while(1){
		vTaskDelay(pdMS_TO_TICKS(1500));
		printf("%d------------------------------------\n", (int) xTaskGetTickCount());
		if(xQueueReceive(active_list_queue, &activeHead, 0)){
			count = get_active_dd_task_list(activeHead);
			printf("Active tasks: %d\n", count);
		}
		else{
			printf("Active tasks: 0\n");
		}

		if(xQueueReceive(complete_list_queue, &completeHead, 0)){
			count = get_complete_dd_task_list(completeHead);
			printf("Completed tasks: %d\n", count);
		}
		else{
			printf("Completed tasks: 0\n");
		}

		if(xQueueReceive(overdue_list_queue, &overdueHead, 0)){
			count = get_overdue_dd_task_list(overdueHead);
			printf("Overdue tasks: %d\n", count);
		}
		else{
			printf("Overdue tasks: 0\n");
		}

		printf("%d------------------------------------\n", (int) xTaskGetTickCount());


	}
}

/* DDS Task (third highest priority) receives, releases and completes user-defined tasks. Manages dd task lists */
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

	uint32_t completed_task_id; // receive completed tasks in completed_queue
	int CPUAvailable = 1; // flag for if new nest UDF can run - initially free to run F-Tasks
	while(1)
	{

		// Receive new tasks from generator
		if(xQueueReceive(release_queue, &new_release_task, 0))
		{
			printf("Received task: %d at: %d\n", (int)new_release_task.task_id, (int)xTaskGetTickCount());
			listInsert(activeHead, new_release_task, 1);
		}

		// Complete DDS tasks
		if(xQueueReceive(completed_queue, &completed_task_id, 0))
		{
			CPUAvailable = 1; // reset flag so DDS can release a new UDF task
			struct dd_task removed = dd_delete(activeHead, completed_task_id);

			 // F-Task finished running, next F-Task can run
			printf("Completed task: %d at: %d\n", (int)removed.task_id, (int)removed.completion_time);

			// Check if task completed on time or not
			if(removed.completion_time < removed.absolute_deadline){
				listInsert(completeHead, removed, 0);
			}
			else{
				printf("Overdue Task: %d at: %d\n", (int)removed.task_id, (int)xTaskGetTickCount());
				listInsert(overdueHead, removed, 0);
			}
		}

		// Modify Active list for any overdue (move overdue task from active list to overdue list)
		dd_remove_overdue(activeHead, overdueHead);

		// Release new task if CPU available (no current DDS task running) and no newly released tasks to process
		if(activeHead->next != NULL && CPUAvailable && (xQueuePeek(release_queue, &new_release_task, 0) == pdFALSE)){

			CPUAvailable = 0; // turn off flag so that no new UDF tasks can be released until this one is done
			dd_create(activeHead); // create the UDF from the head of the active dd task list

		}

		// Write dd task lists via queues to be received by Monitor task
		if(activeHead->next != NULL){
			if(!xQueueOverwrite(active_list_queue, &activeHead)){
				printf("Failed to send new dd Task to active list queue\n");
			}
		}
		if(overdueHead->next != NULL){
			if(!xQueueOverwrite(overdue_list_queue, &overdueHead)){
				printf("Failed to send new dd Task to overdue list queue\n");
			}
		}
		if(completeHead->next != NULL){
			if(!xQueueOverwrite(complete_list_queue, &completeHead)){
				printf("Failed to send new dd Task to complete list queue\n");
			}
		}
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

/* UDF Task (same priority as DDS when running) - tasks in the dd_task struct, runs for e execution time,
 * sets the completion time, sends the task_id back in the completed queue  */
static void User_Defined_Task( void *pvParameters )
{
	struct dd_task* curr_task = (struct dd_task*) pvParameters;
	int startTime = (int)xTaskGetTickCount();
	int endTime = startTime + (int) curr_task->execution_time;
	while ((int)xTaskGetTickCount() < endTime);
	uint32_t task_id = curr_task->task_id;
	curr_task->completion_time = (uint32_t) xTaskGetTickCount();
	complete_dd_task(task_id);
	vTaskSuspend(NULL);
}

/* UDF Task (higher priority than DDS) - releases periodic tasks to be sent to DDS */
static void Deadline_Driven_Task_Generator( void *pvParameters )
{
	uint32_t i = 0;

	while (1){

		if (TEST_BENCH == 1){ // test bench 1
			release_dd_task(0, 1, 95, 500 + i*1500);
			release_dd_task(0, 2, 150, 500 + i*1500);
			release_dd_task(0, 3, 250, 750 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(500));

			release_dd_task(0, 1, 95, 1000 + i*1500);
			release_dd_task(0, 2, 150, 1000 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 3, 250, 1500 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 1500 + i*1500);
			release_dd_task(0, 2, 150, 1500 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(500));
		}
		else if (TEST_BENCH == 2){ // test bench 2
			release_dd_task(0, 1, 95, 250 + i*1500);
			release_dd_task(0, 2, 150, 500 + i*1500);
			release_dd_task(0, 3, 250, 750 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 500 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 750 + i*1500);
			release_dd_task(0, 2, 150, 1000 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 1000 + i*1500);
			release_dd_task(0, 3, 250, 1500 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 1250 + i*1500);
			release_dd_task(0, 2, 150, 1500 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 1500 + i*1500);
			vTaskDelay(pdMS_TO_TICKS(250));
		}
		else { //Test Bench 3
			release_dd_task(0, 1, 100, 500 + i*500);
			release_dd_task(0, 2, 200, 500 + i*500);
			release_dd_task(0, 3, 200, 500 + i*500);
			vTaskDelay(pdMS_TO_TICKS(500));
		}
		i++;
	}
}

/*---------------------------------- DDS FUNCTIONS ------------------------------------------------------------*/
/* DDS Function - Creates a FreeRTOS (UDF) task from the head of the active dd task linked list */
void dd_create(struct dd_task_list* activeHead){
	activeHead->next->task.release_time = (uint32_t)xTaskGetTickCount();
	printf("Released task: %d at: %d\n", (int)activeHead->next->task.task_id, (int)activeHead->next->task.release_time);
	xTaskCreate(User_Defined_Task,"UserDefinedTask",configMINIMAL_STACK_SIZE, &activeHead->next->task, 3, &(activeHead->next->task.t_handle));
}

/* DDS Function - Inserts into the specified dd task list, for active dd task list inserts in sorted order by EDF */
void listInsert(struct dd_task_list* head, struct dd_task new_task, int sort_flag)
{
	struct dd_task_list *new_node = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	new_node->task = new_task;
	new_node->next = NULL;
	struct dd_task_list *curr = head;
    if(sort_flag == 1) // insert in EDF order
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
    else
    {
    	while(curr->next != NULL)
    	{
    		curr = curr->next;
    	}
    }
	curr->next = new_node;
}

/* DDS Function - iterates through the current dd task list and removes the specified node */
struct dd_task dd_delete(struct dd_task_list* head, uint32_t task_id)
{
	struct dd_task_list *curr = head;
	struct dd_task_list *prev = head;
	while (curr->task.task_id != task_id ){
		prev = curr;
		curr = curr->next;
	}
	if (curr->next == NULL){
		prev->next = NULL;
	} else {
		prev->next = curr->next;
	}
	vTaskDelete(curr->task.t_handle);
	return curr->task;
}

/* Iterates through the current active dd task list */
int get_active_dd_task_list(struct dd_task_list* activeHead)
{
	struct dd_task_list *curr = activeHead;
	int i = 0;
//	printf("Active: H ");
	while (curr->next != NULL ){
		curr = curr->next;
		//printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d\n", (int)curr->task.type,(int)curr->task.task_id, (int)curr->task.absolute_deadline, (int)curr->task.release_time);
//		printf("-> %d ", (int)curr->task.task_id);
		i++;
	}
//	printf("\n");
	return i;
}

/* Iterates through the current complete dd task list */
int get_complete_dd_task_list(struct dd_task_list* completeHead)
{
	struct dd_task_list *curr = completeHead;
	int i = 0;
//	printf("Completed: H ");
	while (curr->next != NULL ){
		curr = curr->next;
		//printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)curr->task.type,(int)curr->task.task_id, (int)curr->task.absolute_deadline, (int)curr->task.release_time, (int)curr->task.completion_time);
//		printf("-> %d ", (int)curr->task.task_id);
		i++;
	}
//	printf("\n");
	return i;
}

/* Iterates through the current overdue dd task list */
int get_overdue_dd_task_list(struct dd_task_list* overdueHead)
{
	struct dd_task_list *curr = overdueHead;
	int i = 0;
//	printf("Overdue: H ");
	while (curr->next != NULL ){
		curr = curr->next;
//		printf("-> %d ", (int)curr->task.task_id);
		//printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)curr->task.type,(int)curr->task.task_id, (int)curr->task.absolute_deadline, (int)curr->task.release_time, (int)curr->task.completion_time);
		i++;
	}
//	printf("\n");
	return i;
}

void dd_remove_overdue(struct dd_task_list* activeHead, struct dd_task_list* overdueHead){
	struct dd_task_list *curr = activeHead;
	struct dd_task_list *prev = activeHead;
	while ( curr != NULL ){

		while (curr != NULL && curr->task.absolute_deadline > (uint32_t) xTaskGetTickCount()){
			prev = curr;
			curr = curr->next;
		}
		if(curr->task.release_time != -1 && curr->task.completion_time == -1){
			prev = curr;
			curr = curr->next;
			continue;
		}

		if(curr == NULL){
			return;
		}

		prev->next = curr->next;
		listInsert(overdueHead, curr->task, 0);
		printf("Overdue Task: %d at: %d\n", (int)curr->task.task_id, (int) xTaskGetTickCount());
		curr = prev->next;
	}
}

/*-----------------------------------------------------------*/

void complete_dd_task(uint32_t task_id){

	if(!xQueueSend(completed_queue, &task_id, 0)){
		printf("Failed to send new dd Task to complete queue\n");
	}
}

void release_dd_task(uint32_t type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline)
{
	struct dd_task new_dd_task;

	new_dd_task.type = type;
	new_dd_task.task_id = task_id;
	new_dd_task.execution_time = execution_time;
	new_dd_task.absolute_deadline = absolute_deadline;
	new_dd_task.release_time = -1; // to be set later
	new_dd_task.completion_time = -1; // to be set later

	if(!xQueueSend(release_queue, &new_dd_task, 100)){
		printf("Failed to send new dd Task to release queue\n");
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
}
