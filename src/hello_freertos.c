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

int main( void )
{
    while(1){;}
    return 0;
}
