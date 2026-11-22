/* =============================================================================
 *  main.c  —  PCM5102A DAC via I2S  |  ESP-IDF v5.x
 * =============================================================================
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

//notas: los bancos (piano y guitarra) se incluyen a través de Instrumentos.h,
//que define ZonaWavetable_t antes que ellos
//#include "SalamanderGrandPiano.h"
//#include "BasicWaves.h"          /* ondas básicas deshabilitadas por ahora */
//tareas y configuraciones
#include "IO_config.h"      /* Define OLED_PIN_SDA/SCK → debe ir ANTES de Task_OLED */
#include "Task_SD.h"        /* SD/SPI + WAV: usado por Task_OLED (lista) y Task_audio (reproducción) */
#include "Task_PCF8574.h"
#include "Task_OLED.h"      /* Define cola_oled (QueueHandle_t global)               */
#include "Task_TiraLED.h"
#include "Task_audio.h"
#include "Prueba_estres.h"  /* PRUEBA ESTRES: va después de Task_OLED.h y Task_audio.h */
#include "Task_Teclado.h"   /* on_key_event usa extern cola_oled → va DESPUÉS        */
#include "Task_Aprendizaje.h"
#include "Inicio.h"

#include "Monitor_memoria.h"

static const char *TAG = "MAIN";

SemaphoreHandle_t i2c_bus_mutex;

/* Cola de audio/teclado (la tarea de audio la consume) */
QueueHandle_t cola_de_datos_del_teclado;
QueueHandle_t cola_de_datos_PCF8574_to_OLED;
QueueHandle_t cola_oled;


void app_main(void)
{
    /* ──  Colas ───────────────────────────────────────────── */
    /* 1 lugar + xQueueOverwrite (Task_Teclado.h): cada mensaje es el estado completo de
       las 32 teclas y el audio usa solo el último. Con 10 lugares y envío sin espera,
       si cambian más de 10 teclas en la misma lectura se pierden los últimos estados. */
    cola_de_datos_del_teclado = xQueueCreate(1, sizeof(uint32_t));
    if (cola_de_datos_del_teclado == NULL) return;

    cola_de_datos_PCF8574_to_OLED = xQueueCreate(4, sizeof(uint32_t));
    if (cola_de_datos_PCF8574_to_OLED == NULL) return;
    
    cola_oled = xQueueCreate(4, sizeof(uint32_t));
    if (cola_oled == NULL) return;    

    i2c_bus_mutex = xSemaphoreCreateMutex();

    extern QueueHandle_t cola_aprendizaje; // Declarar
    cola_aprendizaje = xQueueCreate(10, sizeof(uint32_t));
    if (cola_aprendizaje == NULL) return;

    /* ──  Inicializaciones de hardware ────────────────────── */
    teclado_init();
    oled_init();     /* Crea cola_oled + muestra pantalla de bienvenida */
    tira_led_init();    /* crea cola_tira_led + apaga todos LEDs */
    gpio_init();
    pcf8574_init();

    /* SD/SPI: si falla el montaje no se aborta el arranque, solo queda
       sd_montada=false y la prueba 5 lo informa en pantalla */
    if (!sd_init()) {
        ESP_LOGW(TAG, "SD no disponible, la prueba 5 (SD/WAV) quedará deshabilitada");
    }

    /* ──   Tarea de teclado — núcleo 0, prioridad 5 ─────────── */
    xTaskCreatePinnedToCore(
        tarea_teclado, "tarea_teclado",
        2048, NULL, 10, NULL, 0
    );



    /* ──  Tarea OLED — núcleo 0, prioridad 3 ──────────────── */
    xTaskCreatePinnedToCore(
        tarea_oled, "tarea_oled",
        4096, NULL, 9, NULL, 0
    );

    xTaskCreatePinnedToCore(
        tarea_pcf8574, "tarea_pcf",
        2048, NULL, 8, NULL, 0
    );

    /* ── Tarea tira LED — núcleo 0, prioridad 2 ──────────── */
    xTaskCreatePinnedToCore(
        tarea_tira_led, "tarea_tira",
        2048, NULL, 2, NULL, 0
    );

    /* ── Inicializar I2S ──────────────────────────────────── */
    i2s_chan_handle_t tx_chan = i2s_init();
    ESP_LOGI(TAG, "Driver I2S listo. Lanzando tarea de audio...");

    /* ── Tarea de audio — núcleo 1, prioridad máxima ─────── */
    xTaskCreatePinnedToCore(
        audio_playback_task, "audio_task",
        4096, (void *)tx_chan,
        configMAX_PRIORITIES - 1, NULL, 1
    );

    xTaskCreatePinnedToCore(
        tarea_aprendizaje, "tarea_aprendizaje",
        4096, NULL, 4, NULL, 0
    );

    if (xTaskCreatePinnedToCore(tarea_inicio, "tarea_inicio",
                                3072, NULL, 11, NULL, 0) != pdPASS) {
        inicio_activo = false;   // si falla, no dejar el sintetizador mudo
    };

    /* Pruebas de medición: desactivadas por defecto. Se activan por separado con
       PRUEBA_ESTRES (Prueba_estres.h) y MONITOR_MEMORIA (Monitor_memoria.h). */
#if PRUEBA_ESTRES
    /* ── PRUEBA ESTRES — núcleo 0, prioridad mínima (el audio queda solo en el núcleo 1) ── */
    xTaskCreatePinnedToCore(
        tarea_prueba_estres, "prueba_estres",
        4096, NULL, 1, NULL, 0
    );
#endif

#if MONITOR_MEMORIA
    /* ── MONITOR DE MEMORIA — núcleo 0, prioridad mínima ── */
    xTaskCreatePinnedToCore(tarea_monitor_memoria, "monitor_memoria",
                            MM_PILA_MONITOR, NULL, 1, NULL, 0);
#endif
}
