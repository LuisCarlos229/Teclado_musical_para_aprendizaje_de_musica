/* =============================================================================
 *  Inicio.h  —  Secuencia de arranque (se ejecuta una sola vez al encender)
 *
 *  - Barrido de color a lo largo de los 32 LEDs WS2812B (de LED 0 a LED 31).
 *  - Sonido de inicio: senoidal con barrido de frecuencia 50 Hz -> 1000 Hz
 *    (exponencial en el tiempo, es decir, igual de "lineal" al oído por octava).
 *  - Duración total: INICIO_DURACION_MS (2 s).
 *
 *  Reparto de responsabilidades (para no pelear por los recursos):
 *    - LEDs : los maneja esta tarea. tarea_tira_led queda en espera mientras
 *             inicio_activo == true.
 *    - Audio: solo audio_playback_task puede escribir en el I2S. Esta tarea
 *             únicamente le pide arrancar (inicio_audio_go) y la tarea de audio
 *             llama a inicio_audio_llenar_buffer() en lugar de sintetizar.
 *
 *  Al terminar: apaga la tira, baja inicio_activo y se elimina a sí misma.
 * =============================================================================
 */

#ifndef INICIO_H
#define INICIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "IO_config.h"       /* SAMPLE_RATE_HZ                                  */
#include "Task_TiraLED.h"    /* tira_led_handle, TIRA_NUM_LEDS, led_strip_*()   */

#include "Monitor_memoria.h"

/* ──────────────────────────────────────────────────────────────
 *  PARÁMETROS CONFIGURABLES
 * ────────────────────────────────────────────────────────────── */
#define INICIO_DURACION_MS      2000
#define INICIO_VOLUMEN          0.25f    /* Intensidad: 0.0 a 1.0 (fracción del máximo) */
#define INICIO_FREC_INICIAL_HZ  50.0f
#define INICIO_FREC_FINAL_HZ    1000.0f
#define INICIO_FADE_MS          30       /* Rampa de entrada/salida (evita clics)       */

#define INICIO_LED_COLA         8        /* Largo de la estela del barrido [LEDs]       */
#define INICIO_LED_BRILLO       255       /* Brillo máx. por canal (0-255), igual que /3 */
#define INICIO_FRAME_MS         20       /* Período de refresco de la tira (50 FPS)     */

#define INICIO_TWO_PI           6.283185307179586f

/* ──────────────────────────────────────────────────────────────
 *  ESTADO COMPARTIDO
 * ────────────────────────────────────────────────────────────── */
/* true desde el reset hasta que termina la secuencia. Mientras sea true:
 *   - tarea_tira_led no toca la tira
 *   - audio_playback_task emite el sonido de inicio en vez de sintetizar */
volatile bool inicio_activo = true;

static volatile bool inicio_audio_go  = false;  /* la tarea de inicio pide arrancar el sonido */
static volatile bool inicio_audio_fin = false;  /* la tarea de audio avisa que terminó        */

/* Estado interno del generador (solo lo toca la tarea de audio) */
static uint32_t inicio_n    = 0;      /* muestra actual dentro del barrido */
static float    inicio_fase = 0.0f;   /* fase del oscilador [0, 2*pi)      */

/* ──────────────────────────────────────────────────────────────
 *  AUDIO — llamada desde audio_playback_task, una vez por buffer DMA
 *  destino: buffer PCM16 estéreo intercalado (L,R,L,R...) de 'frames' frames.
 *  Frecuencia instantánea: f(n) = f0 * (f1/f0)^(n/N)   (barrido exponencial)
 * ────────────────────────────────────────────────────────────── */
static void inicio_audio_llenar_buffer(int16_t *destino, size_t frames)
{
    if (!inicio_audio_go || inicio_audio_fin) {
        memset(destino, 0, frames * 2 * sizeof(int16_t));
        return;
    }

    const uint32_t total = (uint32_t)(((uint64_t)SAMPLE_RATE_HZ * INICIO_DURACION_MS) / 1000u);
    const uint32_t fade  = (uint32_t)(((uint64_t)SAMPLE_RATE_HZ * INICIO_FADE_MS) / 1000u);
    const float    amp   = 32767.0f * INICIO_VOLUMEN;
    const float    k     = INICIO_FREC_FINAL_HZ / INICIO_FREC_INICIAL_HZ;
    const float    r     = powf(k, 1.0f / (float)total);   /* razón por muestra */

    /* Se recalcula f al inicio de cada buffer con powf() para que el error de
       redondeo de la multiplicación sucesiva no se acumule. */
    float f = INICIO_FREC_INICIAL_HZ * powf(k, (float)inicio_n / (float)total);

    for (size_t i = 0; i < frames; i++) {
        int16_t s = 0;

        if (inicio_n < total) {
            float env = 1.0f;
            if (inicio_n < fade)               env = (float)inicio_n / (float)fade;
            else if (total - inicio_n < fade)  env = (float)(total - inicio_n) / (float)fade;

            s = (int16_t)(sinf(inicio_fase) * amp * env);

            inicio_fase += INICIO_TWO_PI * f / (float)SAMPLE_RATE_HZ;
            if (inicio_fase >= INICIO_TWO_PI) inicio_fase -= INICIO_TWO_PI;
            f *= r;
            inicio_n++;
        }

        destino[i * 2]     = s;
        destino[i * 2 + 1] = s;
    }

    if (inicio_n >= total) inicio_audio_fin = true;
}

/* ──────────────────────────────────────────────────────────────
 *  LEDs — barrido
 * ────────────────────────────────────────────────────────────── */
/* Color puro según la posición del LED: cada canal vale 0 o INICIO_LED_BRILLO,
 * nunca un valor intermedio (sin PWM interno en los WS2812B).
 * 6 tramos a lo largo de la tira: rojo, amarillo, verde, cian, azul, magenta. */
static void inicio_color_puro(int i, uint8_t *r, uint8_t *g, uint8_t *b)
{
    static const uint8_t paleta[6][3] = {
        {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}, {1, 0, 1}
    };
    const int s = (i * 6) / TIRA_NUM_LEDS;          /* 0 a 5 */
    *r = paleta[s][0] ? INICIO_LED_BRILLO : 0;
    *g = paleta[s][1] ? INICIO_LED_BRILLO : 0;
    *b = paleta[s][2] ? INICIO_LED_BRILLO : 0;
}

/* Dibuja un cuadro. progreso: 0.0 (inicio) a 1.0 (fin).
 * Un bloque de INICIO_LED_COLA LEDs encendidos avanza del LED 0 al LED 31
 * y termina de salir de la tira justo en progreso = 1.0. */
static void inicio_dibujar_frame(float progreso)
{
    const float cabeza = progreso * (float)(TIRA_NUM_LEDS - 1 + INICIO_LED_COLA);

    for (int i = 0; i < TIRA_NUM_LEDS; i++) {
        float d = cabeza - (float)i;   /* distancia detrás de la cabeza */

        if (d >= 0.0f && d < (float)INICIO_LED_COLA) {
            uint8_t r, g, b;
            inicio_color_puro(i, &r, &g, &b);
            led_strip_set_pixel(tira_led_handle, i, r, g, b);
        } else {
            led_strip_set_pixel(tira_led_handle, i, 0, 0, 0);
        }
    }
    led_strip_refresh(tira_led_handle);
}

/* ──────────────────────────────────────────────────────────────
 *  TAREA FreeRTOS (one-shot)
 *  El avance se calcula con esp_timer, no contando cuadros: así el barrido
 *  dura INICIO_DURACION_MS aunque otra tarea retrase algún cuadro.
 * ────────────────────────────────────────────────────────────── */
static void tarea_inicio(void *arg)
{
    (void)arg;
    const int64_t duracion_us = (int64_t)INICIO_DURACION_MS * 1000;

    inicio_audio_go = true;                         /* arranca el sonido */
    const int64_t t0 = esp_timer_get_time();

    for (;;) {
        int64_t dt = esp_timer_get_time() - t0;
        if (dt >= duracion_us) break;
        inicio_dibujar_frame((float)dt / (float)duracion_us);
        vTaskDelay(pdMS_TO_TICKS(INICIO_FRAME_MS));
    }
    led_strip_clear(tira_led_handle);

    /* Esperar a que el audio entregue su último buffer (tope 500 ms) */
    for (int i = 0; i < 50 && !inicio_audio_fin; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    inicio_activo = false;
    monitor_pila_al_terminar();
    vTaskDelete(NULL);
}

#endif /* INICIO_H */
