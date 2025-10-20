/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "semphr.h"

int count = 0;
bool on = false;

#define SUPERVISOR_TASK_PRIORITY      ( tskIDLE_PRIORITY + 5UL )
#define LOWER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1UL )
#define HIGHER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 3UL )
#define SUPERVISOR_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define LOWER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define HIGHER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

void lower_prio_task(void *params) {
    hard_assert(cyw43_arch_init() == PICO_OK);
    SemaphoreHandle_t lock = (SemaphoreHandle_t)params;
    xSemaphoreTake(lock, 0xffff);
    while (true) {
        ;
    }
}

void higher_prio_task(void *params) {
    hard_assert(cyw43_arch_init() == PICO_OK);
    SemaphoreHandle_t lock = (SemaphoreHandle_t)params;
    xSemaphoreTake(lock, 0xffff);
    while (true) {
        ;
    }
}

void supervisor_task(__unused void *params) {
    SemaphoreHandle_t lock = xSemaphoreCreateBinary();
    TaskHandle_t lower_task;
    TaskHandle_t higher_task;
    xTaskCreate(lower_prio_task, "LowerPriorityThread",
                LOWER_TASK_STACK_SIZE, (void *)&lock, LOWER_TASK_PRIORITY, &lower_task);
    vTaskDelay(0xff);
    xTaskCreate(higher_prio_task, "HigherPriorityThread",
                HIGHER_TASK_STACK_SIZE, (void *)&lock, HIGHER_TASK_PRIORITY, &higher_task);
    TaskStatus_t lower_task_status;
    TaskStatus_t higher_task_status;

    vTaskGetInfo(lower_task, &lower_task_status, pdFALSE, eInvalid);
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf("Lower Task Status:\nState:%s\nName:%s\n", lower_task_status.eCurrentState, lower_task_status.pcTaskName);
    }
}

int main( void )
{
    stdio_init_all();
    const char *rtos_name;
    rtos_name = "FreeRTOS";
    TaskHandle_t task;
    xTaskCreate(supervisor_task, "SupervisorThread",
                SUPERVISOR_TASK_STACK_SIZE, NULL, SUPERVISOR_TASK_PRIORITY, &task);
    vTaskStartScheduler();
    return 0;
}
