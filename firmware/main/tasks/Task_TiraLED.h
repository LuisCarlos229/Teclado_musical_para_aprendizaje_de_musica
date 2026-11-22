/* =============================================================================
 *  Task_TiraLED.h  —  Tira LED WS2812B via RMT  |  ESP-IDF v5/v6
 *
 *  Driver : led_strip (componente oficial ESP-IDF)
 *  Pin    : GPIO 12  (DIN de la tira)
 *  LEDs   : 32 LEDs WS2812B
 * =============================================================================
 */

#ifndef TASK_TIRA_LED_H
#define TASK_TIRA_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "led_strip.h"

/* ──────────────────────────────────────────────────────────────
 *  CONFIGURACIÓN DE LA TIRA
 * ────────────────────────────────────────────────────────────── */
#define TIRA_PIN_DIN     GPIO_NUM_12   
#define TIRA_NUM_LEDS    32            

/* Brillo máximo para el reflejo del teclado (0–255) */
#define TIRA_BRILLO_R    255
#define TIRA_BRILLO_G    255
#define TIRA_BRILLO_B    255

/* Variables Externas importadas desde Task_OLED.h */
extern volatile uint8_t modo_funcionamiento;
extern volatile uint8_t subestado_prueba;
extern volatile bool inicio_activo;

static led_strip_handle_t tira_led_handle;
QueueHandle_t cola_tira_led;   

/* ──────────────────────────────────────────────────────────────
 *  INICIALIZACIÓN DE LA TIRA
 * ────────────────────────────────────────────────────────────── */
static inline void tira_led_init(void)
{
    led_strip_config_t config = {
        .strip_gpio_num         = TIRA_PIN_DIN,
        .max_leds               = TIRA_NUM_LEDS,
        .led_model              = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10000000,     
        .flags = {
            .with_dma = false,
        },
    };

    cola_tira_led = xQueueCreate(10, sizeof(uint32_t));
    led_strip_new_rmt_device(&config, &rmt_config, &tira_led_handle);
    led_strip_clear(tira_led_handle);  
}

/* ──────────────────────────────────────────────────────────────
 *  FUNCIÓN 1 — REFLEJO DEL TECLADO (Modo Síntesis/Aprendizaje)(en construccion)
 * ────────────────────────────────────────────────────────────── */
static void tira_reflejar_teclado(uint32_t tabla_estado)
{
    for (int i = 0; i < TIRA_NUM_LEDS; i++) {
        if ((tabla_estado >> i) & 1u)
            /* Tecla presionada -> Ilumina en Azul */
            led_strip_set_pixel(tira_led_handle, i, 0, 0, TIRA_BRILLO_B);   
        else
            /* Tecla liberada -> Apagado */
            led_strip_set_pixel(tira_led_handle, i, 0, 0, 0);  
    }
    led_strip_refresh(tira_led_handle);
}

/* ──────────────────────────────────────────────────────────────
 *  FUNCIÓN 2 — ANIMACIÓN MODO PRUEBA (colores primarios, pasos lentos)
 *
 *  Toda la tira se enciende en un color primario (rojo -> verde -> azul)
 *  y pasa al siguiente cada TIRA_PRUEBA_PERIODO_MS.
 *
 *  Para no inducir ruido en el altavoz, la trama se envía UNA sola vez
 *  por cambio de color: entre cambios no hay tráfico hacia los WS2812B.
 *
 *  BRILLO: 85, el mismo nivel máximo que la animación original (255 / 3).
 *  Con 255 en los 32 LEDs el consumo se triplicaba (~0,6 A) y el
 *  PCM5102A dejaba de sonar; con 85 el consumo es ~0,2 A, igual que
 *  con la animación original, con la que el tono siempre funcionó.
 * ────────────────────────────────────────────────────────────── */
#define TIRA_PRUEBA_PERIODO_MS   1000
#define TIRA_PRUEBA_BRILLO       255     /* NO superar 85 con los 32 LEDs (consumo) */
#define TIRA_PRUEBA_SALTO        1      /* 1 = todos los LEDs, 2 = uno de cada dos */

static const uint8_t tira_colores_prueba[][3] = {
    { TIRA_PRUEBA_BRILLO, 0, 0 },                    /* rojo  */
    { 0, TIRA_PRUEBA_BRILLO, 0 },                    /* verde */
    { 0, 0, TIRA_PRUEBA_BRILLO },                    /* azul  */
};
#define TIRA_PRUEBA_N_COLORES  (sizeof(tira_colores_prueba) / sizeof(tira_colores_prueba[0]))

/* Carga el color en el buffer y envía UNA trama a la tira */
static void tira_aplicar_color(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < TIRA_NUM_LEDS; i++) {
        if ((i % TIRA_PRUEBA_SALTO) == 0) led_strip_set_pixel(tira_led_handle, i, r, g, b);
        else                              led_strip_set_pixel(tira_led_handle, i, 0, 0, 0);
    }
    led_strip_refresh(tira_led_handle);
}

/* reiniciar = true al entrar a la prueba: arranca en rojo de inmediato.
 * Devuelve sin tocar la tira si todavía no pasó el período. */
static void tira_animacion_prueba(bool reiniciar)
{
    static uint8_t    idx_color   = 0;
    static TickType_t ultimo_paso = 0;
    const  TickType_t ahora       = xTaskGetTickCount();

    if (reiniciar) {
        idx_color = 0;
    } else if ((ahora - ultimo_paso) >= pdMS_TO_TICKS(TIRA_PRUEBA_PERIODO_MS)) {
        idx_color = (uint8_t)((idx_color + 1) % TIRA_PRUEBA_N_COLORES);
    } else {
        return;                                  /* sin cambio: no se envía nada */
    }

    ultimo_paso = ahora;
    tira_aplicar_color(tira_colores_prueba[idx_color][0],
                       tira_colores_prueba[idx_color][1],
                       tira_colores_prueba[idx_color][2]);
}

/* ──────────────────────────────────────────────────────────────
 *  TAREA FreeRTOS
 * ────────────────────────────────────────────────────────────── */
static void tarea_tira_led(void *arg)
{
    extern QueueHandle_t cola_tira_led;
    uint32_t tabla_estado    = 0;
    uint32_t tabla_mostrada  = 0;
    bool     en_prueba       = false;
    bool     forzar_refresco = true;   /* tras el inicio o al salir de la prueba */

    for (;;) {
        if (inicio_activo) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

        /* Prueba 1 del Modo Prueba de Hardware: solo tira LED (sin audio) */
        if (modo_funcionamiento == 2 && subestado_prueba == PRUEBA_LEDS) {

            /* Drenamos la cola del teclado para que no se acumulen eventos
               mientras hacemos la prueba gráfica */
            uint32_t tmp;
            while (xQueueReceive(cola_tira_led, &tmp, 0) == pdTRUE) { }

            tira_animacion_prueba(!en_prueba);   /* primer paso inmediato al entrar */
            en_prueba = true;
            vTaskDelay(pdMS_TO_TICKS(100));      /* solo consulta el reloj; la tira no se toca */

        } else {
            if (en_prueba) {                     /* al salir de la prueba: apagar y redibujar */
                en_prueba = false;
                forzar_refresco = true;
            }

            /* MODO NORMAL: Síntesis o Aprendizaje */
            if (xQueueReceive(cola_tira_led, &tabla_estado, pdMS_TO_TICKS(50)) == pdTRUE) {

                /* Vaciar cola → quedarse con la matriz de estado más reciente */
                uint32_t tmp;
                while (xQueueReceive(cola_tira_led, &tmp, 0) == pdTRUE) {
                    tabla_estado = tmp;
                }
            }

            /* Refrescar solo si cambió algo: sin tráfico continuo hacia la tira */
            if (forzar_refresco || tabla_estado != tabla_mostrada) {
                tira_reflejar_teclado(tabla_estado);
                tabla_mostrada  = tabla_estado;
                forzar_refresco = false;
            }
        }
    }
}

#endif /* TASK_TIRA_LED_H */