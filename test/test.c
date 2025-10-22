#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include <pico/stdlib.h>
#include <stdint.h>
#include <unity.h>
#include "unity_config.h"
#include "semphr.h"
#include "projdefs.h"


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
    // hard_assert(cyw43_arch_init() == PICO_OK);
    SemaphoreHandle_t lock = (SemaphoreHandle_t)params;
    while(1) {
        xSemaphoreTake(lock, 0xffff);
        for(uint32_t i = 0; i < 100000; i++) {;}
        xSemaphoreGive(lock);
    }
}

void medium_prio_task(void *params) {
    // hard_assert(cyw43_arch_init() == PICO_OK);
    while(1) {
        for(uint32_t i = 0; i < 100000; i++) {;}
    }
}

void lower_prio_task(void *params) {
    // hard_assert(cyw43_arch_init() == PICO_OK);
    SemaphoreHandle_t lock = (*(SemaphoreHandle_t*)params);
    printf("Lock Address: %x\n", &lock);
    while (1) {
        xSemaphoreTake(lock, 0xffff);
        for(uint32_t i = 0; i < 100000; i++) {;}
        xSemaphoreGive(lock);
    }
}

void test_priority_inversion() {
    TEST_MESSAGE("Starting test_priority_inversion.");
    SemaphoreHandle_t lock = xSemaphoreCreateBinary();
    printf("Lock Address: %x\n", &lock);
    TaskHandle_t lower_task;
    TaskHandle_t medium_task;
    TaskHandle_t higher_task;
    xTaskCreate(lower_prio_task, "LowerPriorityThread",
                LOWER_TASK_STACK_SIZE, (void *)&lock, LOWER_TASK_PRIORITY, &lower_task);
    TEST_MESSAGE("Created lower_prio_task.");
    vTaskDelay(pdMS_TO_TICKS(5));
    TEST_MESSAGE("Delayed after lower prio.");
    xTaskCreate(higher_prio_task, "HigherPriorityThread",
                HIGHER_TASK_STACK_SIZE, (void *)&lock, HIGHER_TASK_PRIORITY, &higher_task);
    xTaskCreate(medium_prio_task, "MediumPriorityThread",
                MEDIUM_TASK_STACK_SIZE, NULL, MEDIUM_TASK_PRIORITY, &medium_task);
    TEST_MESSAGE("Created other two tasks.");
    TaskStatus_t lower_task_status;
    TaskStatus_t medium_task_status;
    TaskStatus_t higher_task_status;

    // vTaskDelay(0xff);

    vTaskGetInfo(lower_task, &lower_task_status, pdFALSE, eInvalid);
    TEST_MESSAGE("Got lower task info.");
    while(1) {
        printf("Lower Task Status:\nState: %d\nName: %s\nExecution Time: %lu\n", lower_task_status.eCurrentState, lower_task_status.pcTaskName, lower_task_status.ulRunTimeCounter);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
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
