#ifndef TASK_TECLADO_H
#define TASK_TECLADO_H

#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Prueba_estres.h"   /* PRUEBA ESTRES: teclas virtuales */

// ---------------------------------------------------------------------------
// Parámetros de antirrebote y escaneo
// ---------------------------------------------------------------------------
#define tiempo_scan   15
#define CONT_Lectura  3

// ---------------------------------------------------------------------------
// Pines
// ---------------------------------------------------------------------------
static const gpio_num_t columnas[4] = {GPIO_NUM_13, GPIO_NUM_4,
                                       GPIO_NUM_16, GPIO_NUM_27};

static const gpio_num_t filas[8]    = {GPIO_NUM_25, GPIO_NUM_26,
                                       GPIO_NUM_32, GPIO_NUM_33,
                                       GPIO_NUM_34, GPIO_NUM_35,
                                       GPIO_NUM_36, GPIO_NUM_39};

// ---------------------------------------------------------------------------
// Estado interno
// ---------------------------------------------------------------------------
static uint32_t Tabla_estado = 0;
static uint8_t  Contador[32] = {0};

// ---------------------------------------------------------------------------
// Callback de evento
// ---------------------------------------------------------------------------
static void on_key_event(int col, int row, int pressed, int tecla_num)
{
    if (pressed){
        extern QueueHandle_t cola_de_datos_del_teclado;
        xQueueOverwrite(cola_de_datos_del_teclado, &Tabla_estado);   /* cola de 1 lugar: el audio recibe siempre el último estado */

        extern QueueHandle_t cola_oled;
        xQueueSend(cola_oled, &tecla_num, 0);
        
        /* NUEVO: Enviar tecla a la FSM de Aprendizaje */
        extern QueueHandle_t cola_aprendizaje;
        xQueueSend(cola_aprendizaje, &tecla_num, 0);
    }else{
        extern QueueHandle_t cola_de_datos_del_teclado;
        xQueueOverwrite(cola_de_datos_del_teclado, &Tabla_estado);   /* cola de 1 lugar: el audio recibe siempre el último estado */
    }
    
    /* MODIFICACIÓN: Si estamos en aprendizaje, no enviar la Tabla a la Tira LED
       porque la tarea_aprendizaje controla la tira iluminando el camino */
    extern volatile uint8_t modo_funcionamiento;
    if (modo_funcionamiento != 0) {
        extern QueueHandle_t cola_tira_led;
        xQueueSend(cola_tira_led, &Tabla_estado, 0);
    }
}

// ---------------------------------------------------------------------------
// Escaneo matricial
// ---------------------------------------------------------------------------
static void escanear_teclado(void)
{
    for (int c = 0; c < 4; c++) {
        int tecla = c;
        gpio_set_level(columnas[c], 1);

        for (int f = 0; f < 8; f++) {
            uint8_t estado_actual   = (uint8_t)gpio_get_level(filas[f]);
            estado_actual |= (uint8_t)prueba_estres_tecla(tecla);   /* PRUEBA ESTRES: tecla virtual */
            uint8_t estado_anterior = (Tabla_estado >> tecla) & 1;

            if (estado_actual == estado_anterior) {
                Contador[tecla] = 0;
            } else {
                Contador[tecla]++;
                if (Contador[tecla] >= CONT_Lectura) {
                    if (estado_actual)
                        Tabla_estado |=  (1UL << tecla);
                    else
                        Tabla_estado &= ~(1UL << tecla);

                    Contador[tecla] = 0;
                    on_key_event(c, f, estado_actual, tecla);
                }
            }
            tecla += 4;
        }

        gpio_set_level(columnas[c], 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ---------------------------------------------------------------------------
// Tarea FreeRTOS
// ---------------------------------------------------------------------------
static void tarea_teclado(void *arg)
{
    for (;;) {
        escanear_teclado();
        vTaskDelay(pdMS_TO_TICKS(tiempo_scan));
    }
}

// ---------------------------------------------------------------------------
// Configuración de pines GPIO
// ---------------------------------------------------------------------------
static void configurar_pines(void)
{
    for (int f = 0; f < 8; f++) {
        gpio_set_direction(filas[f], GPIO_MODE_INPUT);
        if (filas[f] <= GPIO_NUM_33)
            gpio_set_pull_mode(filas[f], GPIO_PULLDOWN_ONLY);
    }
    for (int c = 0; c < 4; c++) {
        gpio_set_direction(columnas[c], GPIO_MODE_OUTPUT);
        gpio_set_level(columnas[c], 0);
    }
}

// ---------------------------------------------------------------------------
// Inicialización pública
// ---------------------------------------------------------------------------
static inline void teclado_init(void)
{
    for (int i = 0; i < 32; i++) Contador[i] = 0;
    Tabla_estado = 0;
    configurar_pines();
}

#endif // TASK_TECLADO_H
