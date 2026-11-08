/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>


#include <stdio.h>
#include <string.h> 
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

const int BTN_PIN_R = 28;
const int BTN_PIN_Y = 21;

const int LED_PIN_R = 5;
const int LED_PIN_Y = 10;

QueueHandle_t xQueueButId;
QueueHandle_t xQueueButId_Callback; 
QueueHandle_t xQueueButId_2;

void btn_callback(uint gpio, uint32_t events) {
    if(gpio == BTN_PIN_R && events == 0x4) {
        xQueueSendFromISR(xQueueButId_Callback, &gpio, 0);
    }
    if(gpio == BTN_PIN_Y && events == 0x4) {
        xQueueSendFromISR(xQueueButId_Callback, &gpio, 0);
    }
}

void led_1_task(void *p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    
    int delay;
    int led_state_r = 0;
    while (true) {
        if(led_state_r == 0) {
            if(xQueueReceive(xQueueButId, &delay, portMAX_DELAY)) {
                led_state_r = !led_state_r;
            }
        } else {
            if(xQueueReceive(xQueueButId, &delay, 0)) {
                led_state_r = !led_state_r;
            }
        }

        if(led_state_r == 1) {
            gpio_put(LED_PIN_R, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_put(LED_PIN_R, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        } else {
            gpio_put(LED_PIN_R, 0);
        }
    }
}

void led_2_task(void *p) {
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);

    int delay;
    int led_state_y = 0;
    while (true) {
        if(led_state_y == 0) {
            if(xQueueReceive(xQueueButId_2, &delay, portMAX_DELAY)) {
                led_state_y = !led_state_y;
            }
        } else {
            if(xQueueReceive(xQueueButId_2, &delay, 0)) {
                led_state_y = !led_state_y;
            }
        }

        if(led_state_y == 1) {
            gpio_put(LED_PIN_Y, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_put(LED_PIN_Y, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        } else {
            gpio_put(LED_PIN_Y, 0);
        }
    }
}

void btn_task(void* p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);
    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true, &btn_callback);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    int id;
    int delay = 100;
    while (true) {
        if(xQueueReceive(xQueueButId_Callback, &id, pdMS_TO_TICKS(500))) {
            if(id == BTN_PIN_R) {
                xQueueSend(xQueueButId, &delay, 0);
            }
            if(id == BTN_PIN_Y) {
                xQueueSend(xQueueButId_2, &delay, 0);
            }
        }
    }
}

int main() {
    stdio_init_all();
    xQueueButId = xQueueCreate(32, sizeof(int));
    xQueueButId_2 = xQueueCreate(32, sizeof(int));
    xQueueButId_Callback = xQueueCreate(32, sizeof(int));

    xTaskCreate(led_1_task, "LED_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_2_task, "LED_Task 2", 256, NULL, 1, NULL);

    xTaskCreate(btn_task, "BTN_Task 1", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}