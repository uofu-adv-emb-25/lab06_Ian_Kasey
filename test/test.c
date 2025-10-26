#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include <pico/stdlib.h>
#include <stdint.h>
#include <unity.h>
#include "unity_config.h"
#include "semphr.h"
#include "busy.h"


#define TEST_RUNNER_PRIORITY      ( tskIDLE_PRIORITY + 10UL )
#define LOWER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1UL )
#define MEDIUM_TASK_PRIORITY     ( tskIDLE_PRIORITY + 3UL )
#define HIGHER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 5UL )
#define TEST_RUNNER_STACK_SIZE configMINIMAL_STACK_SIZE
#define LOWER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define MEDIUM_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define HIGHER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

void setUp(void) {}

void tearDown(void) {}

void higher_prio_task(void *params) {
    SemaphoreHandle_t lock = (*(SemaphoreHandle_t*)params);
    while(1) {
        xSemaphoreTake(lock, 0xffff);
        for(uint32_t i = 0; i < 100000; i++) {;}
        xSemaphoreGive(lock);
    }
}

void medium_prio_task(void *params) {
    while(1) {
        for(uint32_t i = 0; i < 100000; i++) {;}
    }
}

void lower_prio_task(void *params) {
    SemaphoreHandle_t lock = (*(SemaphoreHandle_t*)params);
    while (1) {
        xSemaphoreTake(lock, 0xffff);
        for(uint32_t i = 0; i < 100000; i++) {;}
        xSemaphoreGive(lock);
    }
}

void test_priority_inversion() {
    // SemaphoreHandle_t lock = xSemaphoreCreateBinary();
    SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    xSemaphoreGive(lock);
    TaskHandle_t lower_task;
    TaskHandle_t medium_task;
    TaskHandle_t higher_task;
    xTaskCreate(lower_prio_task, "LowerPrioTask",
                LOWER_TASK_STACK_SIZE, (void *)&lock, LOWER_TASK_PRIORITY, &lower_task);
    vTaskDelay(pdMS_TO_TICKS(1));
    xTaskCreate(higher_prio_task, "HigherPrioTask",
                HIGHER_TASK_STACK_SIZE, (void *)&lock, HIGHER_TASK_PRIORITY, &higher_task);
    xTaskCreate(medium_prio_task, "MediumPrioTask",
                MEDIUM_TASK_STACK_SIZE, NULL, MEDIUM_TASK_PRIORITY, &medium_task);
    TaskStatus_t lower_task_status;
    TaskStatus_t medium_task_status;
    TaskStatus_t higher_task_status;

    // Block for 1ms to allow HigherPrioTask & MediumPrioTask to run
    vTaskDelay(pdMS_TO_TICKS(1));

    // eNums for Task States:
    // eRunning == 0
    // eReady   == 1
    // eBlocked == 2

    vTaskGetInfo(lower_task, &lower_task_status, pdFALSE, eInvalid);
    vTaskGetInfo(medium_task, &medium_task_status, pdFALSE, eInvalid);
    vTaskGetInfo(higher_task, &higher_task_status, pdFALSE, eInvalid);
    printf("Lower Task Status:\n\tState: %d\n\tName: %s\n\tExecution Time: %llu\n", lower_task_status.eCurrentState, lower_task_status.pcTaskName, lower_task_status.ulRunTimeCounter);
    printf("Medium Task Status:\n\tState: %d\n\tName: %s\n\tExecution Time: %llu\n", medium_task_status.eCurrentState, medium_task_status.pcTaskName, medium_task_status.ulRunTimeCounter);
    printf("Higher Task Status:\n\tState: %d\n\tName: %s\n\tExecution Time: %llu\n", higher_task_status.eCurrentState, higher_task_status.pcTaskName, higher_task_status.ulRunTimeCounter);

    // Capture current runtime values
    // For each 5ms interval:
    //    lower should stay the same
    //    medium should increase
    //    higher should stay the same
    // Showing priority inversion
    uint64_t lower_prio_runtime = lower_task_status.ulRunTimeCounter;
    uint64_t medium_prio_runtime = medium_task_status.ulRunTimeCounter;
    uint64_t higher_prio_runtime = higher_task_status.ulRunTimeCounter;

    for(int i = 0; i < 4; i++) {
        vTaskDelay(pdMS_TO_TICKS(5));
        vTaskGetInfo(lower_task, &lower_task_status, pdFALSE, eInvalid);
        vTaskGetInfo(medium_task, &medium_task_status, pdFALSE, eInvalid);
        vTaskGetInfo(higher_task, &higher_task_status, pdFALSE, eInvalid);
        printf("Lower Task Status:\n\tState: %d\n\tName: %s\n\tExecution Time: %llu\n", lower_task_status.eCurrentState, lower_task_status.pcTaskName, lower_task_status.ulRunTimeCounter);
        printf("Medium Task Status:\n\tState: %d\n\tName: %s\n\tExecution Time: %llu\n", medium_task_status.eCurrentState, medium_task_status.pcTaskName, medium_task_status.ulRunTimeCounter);
        printf("Higher Task Status:\n\tState: %d\n\tName: %s\n\tExecution Time: %llu\n", higher_task_status.eCurrentState, higher_task_status.pcTaskName, higher_task_status.ulRunTimeCounter);
    }

    vTaskDelete(lower_task);
    vTaskDelete(medium_task);
    vTaskDelete(higher_task);
}

void runner_thread (__unused void *args)
{
    for (;;) {
        printf("Starting test run.\n");
        UNITY_BEGIN();
        RUN_TEST(test_priority_inversion);
        UNITY_END();
        sleep_ms(5000);
    }
}

int main (void)
{
    stdio_init_all();
    sleep_ms(10000);
    printf("Launching runner\n");
    hard_assert(cyw43_arch_init() == PICO_OK);
    xTaskCreate(runner_thread, "TestRunner",
                TEST_RUNNER_STACK_SIZE, NULL, TEST_RUNNER_PRIORITY, NULL);
    vTaskStartScheduler();
	return 0;
}
