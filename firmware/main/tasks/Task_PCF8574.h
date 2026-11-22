/* =============================================================================
 *  Task_PCF8574.h  —  Control de Entradas/Salidas via I2C
 *
 *  Botones (Entradas): P0 (Enter), P1 (Up), P2 (Down), P4 (Back)
 *  LEDs (Salidas activas en ALTO): P3 (Menú), P5 (Res1), P6 (Res2), P7 (Res3)
 * =============================================================================
 */

#ifndef TASK_PCF8574_H
#define TASK_PCF8574_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#define PCF8574_ADDR_WRITE   0x4E
#define PCF8574_ADDR_READ    0x4F

/* Máscara base: P0,P1,P2,P4 como entradas (HIGH), el resto como salidas (inician LOW) */
#define PCF8574_MASK_INPUTS  0x17   /* 0b00010111 */

#define tiempo_scan_pcf      20
#define CONT_Lectura_PCF     3

/* Comandos para la cola OLED */
#define EV_PCF_ENTER_SHORT   1
#define EV_PCF_UP            2
#define EV_PCF_DOWN          3
#define EV_PCF_BACK          4
#define EV_PCF_ENTER_LONG    5

static const uint8_t pcf_bit_mask[4] = { (1<<0), (1<<1), (1<<2), (1<<4) };
static uint8_t Contador_pcf[4] = {0};
static uint8_t estado_pines_anterior[4] = {0};

/* Variables para la pulsación larga del P0 (Enter) */
static uint32_t tiempo_presionado_p0 = 0;
static bool long_press_emitido = false;

/* Variables Globales para encender LEDs desde otras tareas */
volatile bool pcf_led_menu = false;
volatile bool pcf_led_res1 = false;
volatile bool pcf_led_res2 = false;
volatile bool pcf_led_res3 = false;

#define PCF_USE_BUS_MUTEX
#ifdef PCF_USE_BUS_MUTEX
#include "freertos/semphr.h"
extern SemaphoreHandle_t i2c_bus_mutex;
#endif

static inline void pcf_sda(uint8_t v) { gpio_set_level(OLED_PIN_SDA, v); }
static inline void pcf_scl(uint8_t v) { gpio_set_level(OLED_PIN_SCK, v); }
static inline void pcf_delay(void)    { esp_rom_delay_us(5); } 

static void pcf_i2c_start(void) {
    pcf_sda(1); pcf_scl(1); pcf_delay();
    pcf_sda(0);             pcf_delay();   
    pcf_scl(0);             pcf_delay();
}

static void pcf_i2c_stop(void) {
    pcf_sda(0); pcf_scl(0); pcf_delay();
    pcf_scl(1);             pcf_delay();   
    pcf_sda(1);             pcf_delay();   
}

static uint8_t pcf_i2c_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        pcf_sda((byte >> i) & 1); pcf_delay();
        pcf_scl(1); pcf_delay(); pcf_scl(0); pcf_delay();
    }
    gpio_set_direction(OLED_PIN_SDA, GPIO_MODE_INPUT); pcf_delay();
    pcf_scl(1); pcf_delay();
    uint8_t nack = (uint8_t)gpio_get_level(OLED_PIN_SDA);   
    pcf_scl(0); pcf_delay();
    gpio_set_direction(OLED_PIN_SDA, GPIO_MODE_OUTPUT_OD);
    return nack;
}

static uint8_t pcf_i2c_read_byte(uint8_t send_ack) {
    uint8_t data = 0;
    gpio_set_direction(OLED_PIN_SDA, GPIO_MODE_INPUT);
    for (int i = 7; i >= 0; i--) {
        pcf_delay(); pcf_scl(1); pcf_delay();
        data |= (uint8_t)(gpio_get_level(OLED_PIN_SDA) << i);
        pcf_scl(0);
    }
    gpio_set_direction(OLED_PIN_SDA, GPIO_MODE_OUTPUT_OD);
    pcf_sda(send_ack ? 0 : 1); pcf_delay();
    pcf_scl(1); pcf_delay(); pcf_scl(0); pcf_delay();
    pcf_sda(1);             
    return data;
}

static int pcf8574_write(uint8_t value) {
    pcf_i2c_start();
    if (pcf_i2c_write_byte(PCF8574_ADDR_WRITE)) { pcf_i2c_stop(); return -1; }
    if (pcf_i2c_write_byte(value)) { pcf_i2c_stop(); return -1; }
    pcf_i2c_stop();
    return 0;
}

static uint8_t pcf8574_read(void) {
    pcf_i2c_start();
    if (pcf_i2c_write_byte(PCF8574_ADDR_READ)) { pcf_i2c_stop(); return 0xFF; }
    uint8_t data = pcf_i2c_read_byte(0);   
    pcf_i2c_stop();
    return data;
}

static void emitir_comando_oled(uint32_t comando) {
    extern QueueHandle_t cola_de_datos_PCF8574_to_OLED;
    xQueueSend(cola_de_datos_PCF8574_to_OLED, &comando, 0);
}

static void escanear_pcf8574(void)
{
    /*  Componer el byte de salida combinando inputs con el estado de los LEDs */
    uint8_t byte_salida = PCF8574_MASK_INPUTS;
    if (pcf_led_menu) byte_salida |= (1 << 3);
    if (pcf_led_res1) byte_salida |= (1 << 5);
    if (pcf_led_res2) byte_salida |= (1 << 6);
    if (pcf_led_res3) byte_salida |= (1 << 7);
    
    pcf8574_write(byte_salida);

    /* Leer entradas */
    uint8_t byte_leido = pcf8574_read();

    for (int i = 0; i < 4; i++) {
        uint8_t estado_actual = ((byte_leido & pcf_bit_mask[i]) == 0) ? 1u : 0u;

        if (estado_actual == estado_pines_anterior[i]) {
            Contador_pcf[i] = 0;
            
            /* Detección de Pulsación Larga solo para P0 (Enter) */
            if (i == 0 && estado_actual == 1) {
                tiempo_presionado_p0 += tiempo_scan_pcf;
                if (tiempo_presionado_p0 >= 2000 && !long_press_emitido) {
                    emitir_comando_oled(EV_PCF_ENTER_LONG);
                    long_press_emitido = true;
                }
            }
        } else {
            Contador_pcf[i]++;
            if (Contador_pcf[i] >= CONT_Lectura_PCF) {
                /* Confirmar Flanco de Subida / Bajada */
                if (i == 0) { 
                    if (estado_actual == 0) { /* Al soltar Enter */
                        if (!long_press_emitido) emitir_comando_oled(EV_PCF_ENTER_SHORT);
                        tiempo_presionado_p0 = 0;
                        long_press_emitido = false;
                    }
                } else {
                    if (estado_actual == 1) { /* Al presionar flechas o esc */
                        if (i == 1) emitir_comando_oled(EV_PCF_BACK);
                        if (i == 2) emitir_comando_oled(EV_PCF_UP);
                        if (i == 3) emitir_comando_oled(EV_PCF_DOWN);
                    }
                }
                estado_pines_anterior[i] = estado_actual;
                Contador_pcf[i] = 0;
            }
        }
    }
}

static void tarea_pcf8574(void *arg) {
    for (;;) {
#ifdef PCF_USE_BUS_MUTEX
        xSemaphoreTake(i2c_bus_mutex, portMAX_DELAY);
#endif
        escanear_pcf8574();
#ifdef PCF_USE_BUS_MUTEX
        xSemaphoreGive(i2c_bus_mutex);
#endif
        vTaskDelay(pdMS_TO_TICKS(tiempo_scan_pcf)); 
    }
}

static inline void pcf8574_init(void) {
    for (int i = 0; i < 4; i++) { Contador_pcf[i] = 0; estado_pines_anterior[i] = 0; }
    pcf8574_write(PCF8574_MASK_INPUTS);
    printf("==============  pcf8574_init finalizada ===============\n");
}
#endif /* TASK_PCF8574_H */