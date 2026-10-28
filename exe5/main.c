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

#define PISCA_MS     100   /* meio periodo do pisca */
#define DEBOUNCE_MS  50

/* Unicas globais: recursos do RTOS */
QueueHandle_t xQueueBtn;            /* IRQ -> btn_task: qual botao e qual borda */
SemaphoreHandle_t xSemaphoreLedR;   /* btn_task -> led_r_task: alterna o pisca  */
SemaphoreHandle_t xSemaphoreLedY;   /* btn_task -> led_y_task: alterna o pisca  */

/*
 * Callback dos dois botoes. Envia para a fila:
 *   +gpio  -> borda de descida (apertou)
 *   -gpio  -> borda de subida  (soltou)
 * As duas bordas no mesmo evento so acontecem no ricochete: ignora.
 */
void btn_callback(uint gpio, uint32_t events) {
    int evento;

    if (events == GPIO_IRQ_EDGE_FALL) {
        evento = (int)gpio;
    } else if (events == GPIO_IRQ_EDGE_RISE) {
        evento = -(int)gpio;
    } else {
        return;
    }
    xQueueSendFromISR(xQueueBtn, &evento, 0);
}

/*
 * Recebe os eventos da IRQ, filtra o ricochete e libera o semaforo do LED
 * correspondente a cada aperto valido.
 * Debounce: um aperto so vale se a ultima borda daquele botao foi ha mais
 * de 50 ms (o ricochete do aperto e da soltura e descartado).
 */
void btn_task(void* p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    /* Variaveis locais da task (nao sao globais) */
    TickType_t ultimo_r = xTaskGetTickCount() - pdMS_TO_TICKS(1000);
    TickType_t ultimo_y = ultimo_r;
    int evento;

    while (true) {
        if (xQueueReceive(xQueueBtn, &evento, portMAX_DELAY) == pdTRUE) {
            int pino = (evento > 0) ? evento : -evento;
            int apertou = (evento > 0);
            TickType_t agora = xTaskGetTickCount();

            if (pino == BTN_PIN_R) {
                int isolado = (agora - ultimo_r) > pdMS_TO_TICKS(DEBOUNCE_MS);
                ultimo_r = agora;
                if (apertou && isolado) {
                    xSemaphoreGive(xSemaphoreLedR);
                }
            } else if (pino == BTN_PIN_Y) {
                int isolado = (agora - ultimo_y) > pdMS_TO_TICKS(DEBOUNCE_MS);
                ultimo_y = agora;
                if (apertou && isolado) {
                    xSemaphoreGive(xSemaphoreLedY);
                }
            }
        }
    }
}

/*
 * Cada aperto (semaforo) alterna entre "apagado" e "piscando".
 * Apagado: a task dorme no semaforo, sem gastar CPU.
 * Piscando: pisca com 100 ms e confere o semaforo a cada ciclo para parar.
 */
void led_r_task(void* p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 0);

    int piscando = 0;

    while (true) {
        if (!piscando) {
            if (xSemaphoreTake(xSemaphoreLedR, portMAX_DELAY) == pdTRUE) {
                piscando = 1;
            }
            continue;
        }

        gpio_put(LED_PIN_R, 1);
        vTaskDelay(pdMS_TO_TICKS(PISCA_MS));
        gpio_put(LED_PIN_R, 0);
        vTaskDelay(pdMS_TO_TICKS(PISCA_MS));

        if (xSemaphoreTake(xSemaphoreLedR, 0) == pdTRUE) {
            piscando = 0;   /* termina apagado */
        }
    }
}

void led_y_task(void* p) {
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);
    gpio_put(LED_PIN_Y, 0);

    int piscando = 0;

    while (true) {
        if (!piscando) {
            if (xSemaphoreTake(xSemaphoreLedY, portMAX_DELAY) == pdTRUE) {
                piscando = 1;
            }
            continue;
        }

        gpio_put(LED_PIN_Y, 1);
        vTaskDelay(pdMS_TO_TICKS(PISCA_MS));
        gpio_put(LED_PIN_Y, 0);
        vTaskDelay(pdMS_TO_TICKS(PISCA_MS));

        if (xSemaphoreTake(xSemaphoreLedY, 0) == pdTRUE) {
            piscando = 0;   /* termina apagado */
        }
    }
}


int main() {
    stdio_init_all();

    xQueueBtn = xQueueCreate(32, sizeof(int));
    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    xTaskCreate(btn_task, "BTN_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_r_task, "LED_R_Task", 256, NULL, 1, NULL);
    xTaskCreate(led_y_task, "LED_Y_Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}