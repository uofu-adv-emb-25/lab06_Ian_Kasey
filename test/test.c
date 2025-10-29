#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include <pico/stdlib.h>
#include <stdint.h>
#include <unity.h>
#include "unity_config.h"
#include "semphr.h"
#include "busy.h"
#include <stdlib.h>

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
    printf("\n");
}

uint32_t percent_difference(uint32_t a, uint32_t b) {
    uint32_t hi = (a > b) ? a : b;
    uint32_t lo = (a > b) ? b : a;
    if (hi == 0) return 0;
    return ((hi - lo) * 100) / hi;
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

    #ifdef TEST_VERBOSE
    Tasks_To_Print task_list[] = {
        { lower_task, "low priority" },
        { medium_task, "medium priority" },
        { higher_task, "high priority" },
    };

    print_task_status(task_list, 3);
    #endif

    TaskStatus_t lower_prio_status, medium_prio_status, higher_prio_status;

    vTaskGetInfo(lower_task, &lower_prio_status, pdFALSE, eInvalid);
    vTaskGetInfo(medium_task, &medium_prio_status, pdFALSE, eInvalid);
    vTaskGetInfo(higher_task, &higher_prio_status, pdFALSE, eInvalid);


    // Get last runtime value for each task
    uint64_t last_runntime_lower = lower_prio_status.ulRunTimeCounter;
    uint64_t last_runntime_medium = medium_prio_status.ulRunTimeCounter;
    uint64_t last_runntime_higher = higher_prio_status.ulRunTimeCounter;

    // Loop and check each time
    for(int i = 0; i < 5; i ++) {
        vTaskDelay(pdMS_TO_TICKS(1));

        // Refresh values
        vTaskGetInfo(lower_task, &lower_prio_status, pdFALSE, eInvalid);
        vTaskGetInfo(medium_task, &medium_prio_status, pdFALSE, eInvalid);
        vTaskGetInfo(higher_task, &higher_prio_status, pdFALSE, eInvalid);
        
        // Lower prio doesn't change
        TEST_ASSERT_TRUE_MESSAGE(lower_prio_status.ulRunTimeCounter == last_runntime_lower,
                                 "Lower priority runntime should not change for priority inversion.");
        // Medium prio increases
        TEST_ASSERT_TRUE_MESSAGE(medium_prio_status.ulRunTimeCounter > last_runntime_medium,
                                 "Medium priority runntime should increase for priority inversion.");
        // Higher prio doesn't change
        TEST_ASSERT_TRUE_MESSAGE(higher_prio_status.ulRunTimeCounter == last_runntime_higher,
                                 "Higher priority runntime should not change for priority inversion.");
        // Test that Higher Priority is blocked
        TEST_ASSERT_TRUE_MESSAGE(higher_prio_status.eCurrentState == 2,
                                 "Higher priority task should be blocked due to priority inversion.");

        // Set previous runtimes
        last_runntime_lower = lower_prio_status.ulRunTimeCounter;
        last_runntime_medium = medium_prio_status.ulRunTimeCounter;
        last_runntime_higher = higher_prio_status.ulRunTimeCounter;
    }


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

// prediction: both will progress roughly the same
// outcome: true
void test_same_priority_busy_busy(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_busy, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    xTaskCreate(busy_busy, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    #ifdef TEST_VERBOSE            
    Tasks_To_Print task_list[] = {
        { task1, "task 1 (both busy)" },
        { task2, "task 2 (both busy)" },
    };

    print_task_status(task_list, 2);
    #endif

    vTaskDelay(pdMS_TO_TICKS(10));

    TaskStatus_t status1;
    TaskStatus_t status2;

    vTaskGetInfo(task1, &status1, pdFALSE, eInvalid);
    vTaskGetInfo(task2, &status2, pdFALSE, eInvalid);

    // Check task priorities
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task1), uxTaskPriorityGet(task2));
    
    // Check that both are making progress
    TEST_ASSERT_TRUE_MESSAGE(status1.ulRunTimeCounter > 0 && status2.ulRunTimeCounter > 0, "Both should have runtime > 0");

    // Check that they are close to the same execution (this is probably not a great test for systems that have many running tasks, but for 
    // this program it will probably be okay 99% of the time)
    // Check that they are not more than 20% different in run time
    TEST_ASSERT_TRUE_MESSAGE(percent_difference(status1.ulRunTimeCounter, status2.ulRunTimeCounter) < 50 , "Run times differ too much");
    

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

// prediction: both will progress roughly the same, but maybe slower?
// outcome: true, but they weren't really slower
void test_same_priority_busy_yield(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_yield, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    xTaskCreate(busy_yield, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    #ifdef TEST_VERBOSE            
    Tasks_To_Print task_list[] = {
        { task1, "task 1 (both yield)" },
        { task2, "task 2 (both yield)" },
    };

    print_task_status(task_list, 2);
    #endif

    vTaskDelay(pdMS_TO_TICKS(10));

    TaskStatus_t status1;
    TaskStatus_t status2;

    vTaskGetInfo(task1, &status1, pdFALSE, eInvalid);
    vTaskGetInfo(task2, &status2, pdFALSE, eInvalid);

    // Check task priorities
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task1), uxTaskPriorityGet(task2));
    
    // Check that both are making progress
    TEST_ASSERT_TRUE_MESSAGE(status1.ulRunTimeCounter > 0 && status2.ulRunTimeCounter > 0, "Both should have runtime > 0");

    // Check that they are close to the same execution (this is probably not a great test for systems that have many running tasks, but for 
    // this program it will probably be okay 99% of the time)
    // Check that they are not more than 20% different in run time
    TEST_ASSERT_TRUE_MESSAGE(percent_difference(status1.ulRunTimeCounter, status2.ulRunTimeCounter) < 50 , "Run times differ too much");
    

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

// prediction: yield task will not progress while busy task will
// outcome: true
void test_same_priority_busy_yield_and_busy(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_busy, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    xTaskCreate(busy_yield, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    #ifdef TEST_VERBOSE            
    Tasks_To_Print task_list[] = {
        { task1, "task 1 (running busy)" },
        { task2, "task 2 (running yield)" },
    };

    print_task_status(task_list, 2);
    #endif

    vTaskDelay(pdMS_TO_TICKS(10));

    TaskStatus_t status1;
    TaskStatus_t status2;

    vTaskGetInfo(task1, &status1, pdFALSE, eInvalid);
    vTaskGetInfo(task2, &status2, pdFALSE, eInvalid);

    // Check task priorities
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task1), uxTaskPriorityGet(task2));
    

    // Check that task1 is running much more than task2
    TEST_ASSERT_TRUE_MESSAGE(status1.ulRunTimeCounter > status2.ulRunTimeCounter , "yield task had greater execution time");
    TEST_ASSERT_TRUE_MESSAGE(percent_difference(status1.ulRunTimeCounter, status2.ulRunTimeCounter) > 50 , "Run times were too close");
    

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

// prediction: high priority task will complete, then low prio
// outcome: true
void test_different_priority_busy_busy_high_first(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_busy, "task 1",
                HIGHER_TASK_STACK_SIZE, NULL, HIGHER_TASK_PRIORITY, &task1);
    vTaskDelay(pdMS_TO_TICKS(1));
    xTaskCreate(busy_busy, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    #ifdef TEST_VERBOSE            
    Tasks_To_Print task_list[] = {
        { task1, "task 1 (high prio running busy)" },
        { task2, "task 2 (low prio running busy)" },
    };

    print_task_status(task_list, 2);
    #endif

    vTaskDelay(pdMS_TO_TICKS(10));

    TaskStatus_t status1;
    TaskStatus_t status2;

    vTaskGetInfo(task1, &status1, pdFALSE, eInvalid);
    vTaskGetInfo(task2, &status2, pdFALSE, eInvalid);

    // Check task priorities
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task1), HIGHER_TASK_PRIORITY);
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task2), LOWER_TASK_PRIORITY);

    // Check that they differ in run time signifcantly
    TEST_ASSERT_TRUE_MESSAGE(status1.ulRunTimeCounter > status2.ulRunTimeCounter , "yield task had greater execution time");
    TEST_ASSERT_TRUE_MESSAGE(percent_difference(status1.ulRunTimeCounter, status2.ulRunTimeCounter) > 50 , "Run times were too close");

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

// prediction: high priority completes first
// outcome: true
void test_different_priority_busy_busy_low_first(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_busy, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    vTaskDelay(1);
    xTaskCreate(busy_busy, "task 2",
                HIGHER_TASK_STACK_SIZE, NULL, HIGHER_TASK_PRIORITY, &task2);

    #ifdef TEST_VERBOSE            
    Tasks_To_Print task_list[] = {
        { task1, "task 1 (low prio running busy)" },
        { task2, "task 2 (high prio running busy)" },
    };

    print_task_status(task_list, 2);
    #endif

    vTaskDelay(pdMS_TO_TICKS(10));

    TaskStatus_t status1;
    TaskStatus_t status2;

    vTaskGetInfo(task1, &status1, pdFALSE, eInvalid);
    vTaskGetInfo(task2, &status2, pdFALSE, eInvalid);

    // Check task priorities
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task2), HIGHER_TASK_PRIORITY);
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task1), LOWER_TASK_PRIORITY);

    // Check that they differ in run time signifcantly
    TEST_ASSERT_TRUE_MESSAGE(status2.ulRunTimeCounter > status1.ulRunTimeCounter , "yield task had greater execution time");
    TEST_ASSERT_TRUE_MESSAGE(percent_difference(status1.ulRunTimeCounter, status2.ulRunTimeCounter) > 50 , "Run times were too close");

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

// prediction: high prio will complete first as there's nothing to yield to
// outcome: true
void test_different_priority_busy_yield_high_first(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_yield, "task 1",
                HIGHER_TASK_STACK_SIZE, NULL, HIGHER_TASK_PRIORITY, &task1);
    vTaskDelay(pdMS_TO_TICKS(1));
    xTaskCreate(busy_yield, "task 2",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task2);

    #ifdef TEST_VERBOSE            
    Tasks_To_Print task_list[] = {
        { task1, "task 1 (high prio running yield)" },
        { task2, "task 2 (low prio running yield)" },
    };

    print_task_status(task_list, 2);
    #endif

    vTaskDelay(pdMS_TO_TICKS(10));

    TaskStatus_t status1;
    TaskStatus_t status2;

    vTaskGetInfo(task1, &status1, pdFALSE, eInvalid);
    vTaskGetInfo(task2, &status2, pdFALSE, eInvalid);

    // Check task priorities
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task1), HIGHER_TASK_PRIORITY);
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task2), LOWER_TASK_PRIORITY);

    // Check that they differ in run time signifcantly
    TEST_ASSERT_TRUE_MESSAGE(status1.ulRunTimeCounter > status2.ulRunTimeCounter , "yield task had greater execution time");
    TEST_ASSERT_TRUE_MESSAGE(percent_difference(status1.ulRunTimeCounter, status2.ulRunTimeCounter) > 50 , "Run times were too close");

    vTaskDelete(task1);
    vTaskDelete(task2);

    // give time for cleanup
    vTaskDelay(pdMS_TO_TICKS(1));
}

// prediction: low prio will execute for a small time, but then high prio will complete
// outcome: true
void test_different_priority_busy_yield_low_first(void) {
    TaskHandle_t task1;
    TaskHandle_t task2;

    xTaskCreate(busy_yield, "task 1",
                LOWER_TASK_STACK_SIZE, NULL, LOWER_TASK_PRIORITY, &task1);
    vTaskDelay(pdMS_TO_TICKS(1));
    xTaskCreate(busy_yield, "task 2",
                HIGHER_TASK_STACK_SIZE, NULL, HIGHER_TASK_PRIORITY, &task2);

    #ifdef TEST_VERBOSE            
    Tasks_To_Print task_list[] = {
        { task1, "task 1 (low prio running yield)" },
        { task2, "task 2 (high prio running yield)" },
    };

    print_task_status(task_list, 2);
    #endif

    vTaskDelay(pdMS_TO_TICKS(10));

    TaskStatus_t status1;
    TaskStatus_t status2;

    vTaskGetInfo(task1, &status1, pdFALSE, eInvalid);
    vTaskGetInfo(task2, &status2, pdFALSE, eInvalid);

    // Check task priorities
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task2), HIGHER_TASK_PRIORITY);
    TEST_ASSERT_EQUAL_UINT32(uxTaskPriorityGet(task1), LOWER_TASK_PRIORITY);

    // Check that they differ in run time signifcantly
    TEST_ASSERT_TRUE_MESSAGE(status2.ulRunTimeCounter > status1.ulRunTimeCounter , "yield task had greater execution time");
    TEST_ASSERT_TRUE_MESSAGE(percent_difference(status1.ulRunTimeCounter, status2.ulRunTimeCounter) > 50 , "Run times were too close");

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
        // RUN_TEST(test_priority_inversion_with_mutex);
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
