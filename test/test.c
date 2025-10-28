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

// Uncomment for verbose output
// #define TEST_VERBOSE

void setUp(void) {}

void tearDown(void) {}

typedef struct {
    TaskHandle_t task_handle;
    const char* task_name;
} Tasks_To_Print;

void print_task_status(Tasks_To_Print* task_list, size_t task_count) {
    TaskStatus_t status[task_count];

    // eNums for Task States:
    // eRunning == 0
    // eReady   == 1
    // eBlocked == 2
    for(int i=0; i < 5; i++) {
        for(int i=0; i < task_count; i++) {
            vTaskGetInfo(task_list[i].task_handle, &status[i], pdFALSE, eInvalid);
            printf("Task %s Status:\n\tState: %d\n\tExecution Time: %llu\n", task_list[i].task_name, status[i].eCurrentState, status[i].ulRunTimeCounter);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

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
    SemaphoreHandle_t lock = xSemaphoreCreateBinary();
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

    // Block for 1ms to allow HigherPrioTask & MediumPrioTask to run
    vTaskDelay(pdMS_TO_TICKS(1));

    TaskStatus_t lower_prio_status, medium_prio_status, higher_prio_status;

    vTaskGetInfo(lower_task, &lower_prio_status, pdFALSE, eInvalid);
    vTaskGetInfo(medium_task, &medium_prio_status, pdFALSE, eInvalid);
    vTaskGetInfo(higher_task, &higher_prio_status, pdFALSE, eInvalid);


    // Get last runtime value for each task
    uint64_t last_runntime_lower = lower_prio_status.ulRunTimeCounter;
    uint64_t last_runntime_medium = lower_prio_status.ulRunTimeCounter;
    uint64_t last_runntime_higher = lower_prio_status.ulRunTimeCounter;

    // Loop and check each time
    for(int i = 0; i < 5; i ++) {
        vTaskDelay(pdMS_TO_TICKS(1));
        // Lower prio doesn't change
        TEST_ASSERT_TRUE_MESSAGE(lower_prio_status.ulRunTimeCounter == last_runntime_lower,
                                 "Lower priority runntime should not change for priority inversion.");
        // Medium prio increases
        TEST_ASSERT_TRUE_MESSAGE(medium_prio_status.ulRunTimeCounter > last_runntime_medium,
                                 "Medium priority runntime should increase for priority inversion.");
        // Higher prio doesn't change
        TEST_ASSERT_TRUE_MESSAGE(higher_prio_status.ulRunTimeCounter == last_runntime_higher,
                                 "Higher priority runntime should not change for priority inversion.");

        // Set previous runtimes
        last_runntime_lower = lower_prio_status.ulRunTimeCounter;
        last_runntime_medium = lower_prio_status.ulRunTimeCounter;
        last_runntime_higher = lower_prio_status.ulRunTimeCounter;
    }

    #ifdef TEST_VERBOSE
    Tasks_To_Print task_list[] = {
        { lower_task, "low priority" },
        { medium_task, "medium priority" },
        { higher_task, "high priority" },
    };

    print_task_status(task_list, 3);
    #endif

    // Cleanup
    vTaskDelete(lower_task);
    vTaskDelete(medium_task);
    vTaskDelete(higher_task);
}

void test_priority_inversion_with_mutex() {
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

    // Block for 1ms to allow HigherPrioTask & MediumPrioTask to run
    vTaskDelay(pdMS_TO_TICKS(1));

    Tasks_To_Print task_list[] = {
        { lower_task, "low priority" },
        { medium_task, "medium priority" },
        { higher_task, "high priority" },
    };

    print_task_status(task_list, 3);

    vTaskDelete(lower_task);
    vTaskDelete(medium_task);
    vTaskDelete(higher_task);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

void test_same_priority_busy_busy(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_busy, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    xTaskCreate(busy_busy, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    Tasks_To_Print task_list[] = {
        { task1, "task 1" },
        { task2, "task 2" },
    };

    print_task_status(task_list, 2);

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

void test_same_priority_busy_yield(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_yield, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    xTaskCreate(busy_yield, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    Tasks_To_Print task_list[] = {
        { task1, "task 1" },
        { task2, "task 2" },
    };

    print_task_status(task_list, 2);

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

void test_same_priority_busy_yield_and_busy(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_busy, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    xTaskCreate(busy_yield, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    Tasks_To_Print task_list[] = {
        { task1, "task 1 (busy)" },
        { task2, "task 2 (yield)" },
    };

    print_task_status(task_list, 2);

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

void test_different_priority_busy_busy_high_first(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_busy, "task 1",
                HIGHER_TASK_STACK_SIZE, NULL, HIGHER_TASK_PRIORITY, &task1);
    vTaskDelay(pdMS_TO_TICKS(1));
    xTaskCreate(busy_busy, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    Tasks_To_Print task_list[] = {
        { task1, "task 1 (high)" },
        { task2, "task 2 (low)" },
    };

    print_task_status(task_list, 2);

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

void test_different_priority_busy_busy_low_first(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_busy, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    vTaskDelay(pdMS_TO_TICKS(1));
    xTaskCreate(busy_busy, "task 2",
                HIGHER_TASK_STACK_SIZE, NULL, HIGHER_TASK_PRIORITY, &task2);

    Tasks_To_Print task_list[] = {
        { task1, "task 1 (high)" },
        { task2, "task 2 (low)" },
    };

    print_task_status(task_list, 2);

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

void test_different_priority_busy_yield_high_first(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_yield, "task 1",
                HIGHER_TASK_STACK_SIZE, NULL, HIGHER_TASK_PRIORITY, &task1);
    vTaskDelay(pdMS_TO_TICKS(1));
    xTaskCreate(busy_yield, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    Tasks_To_Print task_list[] = {
        { task1, "task 1 (high)" },
        { task2, "task 2 (low)" },
    };

    print_task_status(task_list, 2);

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

void test_different_priority_busy_yield_low_first(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_yield, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    vTaskDelay(pdMS_TO_TICKS(1));
    xTaskCreate(busy_yield, "task 2",
                HIGHER_TASK_STACK_SIZE, NULL, HIGHER_TASK_PRIORITY, &task2);

    Tasks_To_Print task_list[] = {
        { task1, "task 1 (high)" },
        { task2, "task 2 (low)" },
    };

    print_task_status(task_list, 2);

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

void runner_thread (__unused void *args)
{
    for (;;) {
        printf("Starting test run.\n");
        UNITY_BEGIN();
        RUN_TEST(test_priority_inversion);
        RUN_TEST(test_priority_inversion_with_mutex);
        RUN_TEST(test_same_priority_busy_busy);
        RUN_TEST(test_same_priority_busy_yield);
        RUN_TEST(test_same_priority_busy_yield_and_busy);
        RUN_TEST(test_different_priority_busy_busy_high_first);
        RUN_TEST(test_different_priority_busy_busy_low_first);
        RUN_TEST(test_different_priority_busy_yield_high_first);
        RUN_TEST(test_different_priority_busy_yield_low_first);
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
