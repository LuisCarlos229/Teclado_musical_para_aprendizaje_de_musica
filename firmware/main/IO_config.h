/* =============================================================================
 *  IO_config.h  —  Configuración de pines y parámetros I2S/DMA
 *
 *  PCM5102A DAC via I2S  |  ESP-IDF v5.x
 *
 *  CONEXIONES FÍSICAS
 *  ──────────────────
 *  PCM5102A   →  ESP32
 *  BCK        →  PIN_I2S_BCK   (GPIO 17)
 *  LRCK/WS    →  PIN_I2S_LRCK  (GPIO 22)
 *  DIN        →  PIN_I2S_DATA  (GPIO 21)
 *  FMT        →  GND           (formato I2S estándar)
 *  XSMT       →  3V3           (sin mute)
 *  VCC        →  3V3
 *  GND        →  GND
 * =============================================================================
 */

#ifndef IO_CONFIG_H
#define IO_CONFIG_H

#include "driver/gpio.h"
#include "driver/i2s_std.h"

/* ──────────────────────────────────────────────────────────────
 *  PINES GPIO
 * ────────────────────────────────────────────────────────────── */
#define PIN_I2S_BCK     GPIO_NUM_17   /* Bit Clock                          */
#define PIN_I2S_LRCK    GPIO_NUM_22   /* Left/Right Clock (Word Select)     */
#define PIN_I2S_DATA    GPIO_NUM_21   /* Serial Data Out → DIN del PCM5102A */

//#define PIN_LED         GPIO_NUM_2   /* para prbar comunicacion entre tareas */

/* ──────────────────────────────────────────────────────────────
 *  PINES I2C
 * ────────────────────────────────────────────────────────────── */
#define OLED_PIN_SDA    GPIO_NUM_14
#define OLED_PIN_SCK    GPIO_NUM_15

/* ──────────────────────────────────────────────────────────────
 *  PARÁMETROS DE AUDIO
 * ────────────────────────────────────────────────────────────── */
#define SAMPLE_RATE_HZ   44100        /* Frecuencia de muestreo [Hz]        */
#define WAVETABLE_LEN    58

/* Polifonía máxima: notas que suenan a la vez. Acota la carga del núcleo 1
 * (elegido con Prueba_estres.h dejando margen). Si llega una nota más, se
 * roba una voz (ver voces_hacer_lugar() en Task_audio.h). Rango: 1 a 32. */
#ifndef MAX_VOCES
#define MAX_VOCES        14
#endif

/* ──────────────────────────────────────────────────────────────
 *  PARÁMETROS DEL DRIVER I2S / DMA
 * ────────────────────────────────────────────────────────────── */
#define DMA_BUF_FRAMES   512          /* Muestras por buffer DMA            */
#define DMA_BUF_COUNT    4            /* Número de buffers DMA en cola      */

/* ──────────────────────────────────────────────────────────────
 *  FUNCIÓN DE INICIALIZACIÓN I2S
 *  Crea y habilita el canal I2S TX en modo PLL (sin MCLK externo).
 *  Devuelve el handle del canal listo para usarse.
 * ────────────────────────────────────────────────────────────── */

static void gpio_init(void){
    //gpio_reset_pin(PIN_LED);
    //gpio_set_direction(PIN_LED,GPIO_MODE_OUTPUT);
    //gpio_set_pull_mode(PIN_LED, GPIO_PULLUP_ONLY);//para la pullup interna
    printf("==============  gpio_init finalizada =============== \n");
}

static inline i2s_chan_handle_t i2s_init(void)
{
    i2s_chan_handle_t tx_chan;

    /* Crear canal I2S TX */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_FRAMES;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    /*  Configurar modo estándar I2S (formato Philips/I2S) */
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE_HZ,
            .clk_src        = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple  = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    /* PLL interno: sin MCLK externo */
            .bclk = PIN_I2S_BCK,
            .ws   = PIN_I2S_LRCK,
            .dout = PIN_I2S_DATA,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));

    /* Habilitar el canal (activa las señales de reloj) */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    return tx_chan;
}

#endif /* IO_CONFIG_H */
