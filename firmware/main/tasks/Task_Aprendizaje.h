/* =============================================================================
 * Task_Aprendizaje.h  —  FSM para el Modo Aprendizaje (Binario APR1)
 * =============================================================================
 */

#ifndef TASK_APRENDIZAJE_H
#define TASK_APRENDIZAJE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "Task_SD.h"

/* Archivos estáticos en Flash */
// #include "cancion_bwv801_ch1.h"
// #include "cancion_bwv800_ch1.h"
// #include "cancion_bwv798_ch1.h"
// #include "cancion_bwv794_ch1.h"
// #include "cancion_bwv787_ch1.h"

#include "cancion_feliz_cumple.h"
#include "cancion_estrellita.h"
#include "cancion_oda_alegria.h"
#include "cancion_para_elisa.h"

extern QueueHandle_t cola_tira_led;
QueueHandle_t cola_aprendizaje; 

extern volatile uint8_t modo_funcionamiento;
extern volatile uint16_t metronomo_bpm; /* Variable global de Task_OLED.h */

volatile uint16_t ap_paso_actual = 0;
volatile uint16_t ap_paso_total = 0;
volatile uint8_t  ap_estado_fsm = 0; 

/* Selección de fuente: 0 = SD, 1-5 = Flash */
volatile uint8_t ap_fuente_actual = 1; 

static const char *TAG_AP = "APRENDIZAJE";

/* Punteros y offsets para lectura desde Flash */
static const uint8_t* ap_flash_ptr = NULL;
static uint32_t       ap_flash_len = 0;
static uint32_t       ap_flash_offset = 0;
extern volatile uint8_t ap_sd_indice;

#define APR_HEADER_SIZE 34
#define APR_PPQN        12  /* Ajustar según la resolución temporal real del .apr */

/* Función unificada para leer eventos de 4 bytes desde SD o Flash */
static bool leer_evento_apr(FILE *f, uint8_t *tecla, uint8_t *accion, uint16_t *delta) {
    uint8_t buf[4];
    if (ap_fuente_actual == 0) {
        if (f == NULL || fread(buf, 1, 4, f) != 4) return false;
    } else {
        if (ap_flash_offset + 4 > ap_flash_len) return false;
        memcpy(buf, &ap_flash_ptr[ap_flash_offset], 4);
        ap_flash_offset += 4;
    }
    *tecla  = buf[0];
    *accion = buf[1];
    *delta  = (uint16_t)(buf[2] | (buf[3] << 8));
    return true;
}

static void tarea_aprendizaje(void *arg)
{
    FILE *f = NULL;
    uint8_t tecla, accion;
    uint16_t delta;
    uint32_t leds_guia = 0;
    uint32_t tecla_esperada = 0;

    for (;;) {
        if (modo_funcionamiento != 0) {
            if (f != NULL) { fclose(f); f = NULL; }
            ap_estado_fsm = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        switch (ap_estado_fsm) {
            case 0: /* INICIALIZACIÓN */
                ap_paso_total = 0;
                ap_paso_actual = 0;
                leds_guia = 0;
                ap_flash_offset = APR_HEADER_SIZE; /* Saltar cabecera */

                if (ap_fuente_actual == 0 && sd_montada) {
                    /* Lectura desde SD usando la ruta dinámica */
                    char ruta[64];
                    sd_obtener_ruta_apr(ap_sd_indice, ruta, sizeof(ruta));

                    f = fopen(ruta, "rb");
                    if (f != NULL) {
                        fseek(f, APR_HEADER_SIZE, SEEK_SET);
                        ap_estado_fsm = 1;
                    } else {
                        ESP_LOGE(TAG_AP, "Error al abrir archivo SD: %s", ruta);
                        vTaskDelay(pdMS_TO_TICKS(2000));
                    }
                } else if (ap_fuente_actual > 0) {
                    /* Selección desde Flash */
                    switch(ap_fuente_actual) {
                        case 1: ap_flash_ptr = cancion_feliz_cumple; ap_flash_len = CANCION_FELIZ_CUMPLE_SIZE; break;
                        case 2: ap_flash_ptr = cancion_estrellita; ap_flash_len = CANCION_ESTRELLITA_SIZE; break;
                        case 3: ap_flash_ptr = cancion_oda_alegria; ap_flash_len = CANCION_ODA_ALEGRIA_SIZE; break;
                        case 4: ap_flash_ptr = cancion_para_elisa; ap_flash_len = CANCION_PARA_ELISA_SIZE; break;
                        //case 1: ap_flash_ptr = cancion_bwv801_ch1; ap_flash_len = CANCION_BWV801_CH1_SIZE; break;
                        //case 2: ap_flash_ptr = cancion_bwv800_ch1; ap_flash_len = CANCION_BWV800_CH1_SIZE; break;
                        //case 3: ap_flash_ptr = cancion_bwv798_ch1; ap_flash_len = CANCION_BWV798_CH1_SIZE; break;
                        //case 4: ap_flash_ptr = cancion_bwv794_ch1; ap_flash_len = CANCION_BWV794_CH1_SIZE; break;
                        //case 5: ap_flash_ptr = cancion_bwv787_ch1; ap_flash_len = CANCION_BWV787_CH1_SIZE; break;
                        //default: ap_flash_ptr = cancion_bwv801_ch1; ap_flash_len = CANCION_BWV801_CH1_SIZE; break;
                        default: ap_flash_ptr = cancion_para_elisa; ap_flash_len = CANCION_PARA_ELISA_SIZE; break;
                    }
                    /* Opcional: pre-escaneo rápido para contar notas (ap_paso_total) */
                    ap_estado_fsm = 1;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                break;

            case 1: /* LECTURA Y REPRODUCCIÓN DINÁMICA */
                if (leer_evento_apr(f, &tecla, &accion, &delta)) {
                    
                    /* Retardo dinámico dependiente de BPM */
                    if (delta > 0) {
                        uint32_t delay_ms = (delta * 60000UL) / (metronomo_bpm * APR_PPQN);
                        if (delay_ms > 0) {
                            vTaskDelay(pdMS_TO_TICKS(delay_ms));
                        }
                    }

                    if (accion == 3) { /* Presionar */
                        leds_guia |= (1UL << tecla);
                    } else if (accion == 0) { /* Soltar */
                        leds_guia &= ~(1UL << tecla);
                    }
                    
                    xQueueSend(cola_tira_led, &leds_guia, pdMS_TO_TICKS(10));

                    /* Pausar y esperar al usuario si es una nota objetivo */
                    if (accion == 3) { 
                        tecla_esperada = tecla;
                        ap_estado_fsm = 2;
                    }
                } else {
                    /* EOF alcanzado */
                    if (f != NULL) { fclose(f); f = NULL; }
                    leds_guia = 0;
                    xQueueSend(cola_tira_led, &leds_guia, 0);
                    ap_estado_fsm = 3;
                }
                break;

            case 2: /* ESPERA INTERACTIVA */
                {
                    uint32_t tecla_presionada;
                    if (xQueueReceive(cola_aprendizaje, &tecla_presionada, pdMS_TO_TICKS(100)) == pdTRUE) {
                        if (tecla_presionada == tecla_esperada) {
                            leds_guia &= ~(1UL << tecla_esperada);
                            xQueueSend(cola_tira_led, &leds_guia, 0);
                            ap_paso_actual++;
                            ap_estado_fsm = 1;
                        }
                    }
                }
                break;

            case 3: /* FIN */
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}

#endif /* TASK_APRENDIZAJE_H */