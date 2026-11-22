/* =============================================================================
 *  Task_SD.h  —  Lectura de tarjeta SD (modo SPI) y reproducción directa de WAV
 *
 *  Uso: prueba rápida de HW/SW dentro del modo de prueba existente
 *       (subestado_prueba == 5). No reemplaza al motor de síntesis: solo
 *       "puentea" el buffer i2s_tx_buf con datos leídos crudos del archivo,
 *       ya que ambos .wav de prueba son PCM estéreo de 16 bits (mismo layout
 *       que el buffer de salida I2S).
 *
 *  CONEXIONES FÍSICAS (módulo SD por SPI — pines VSPI de fábrica, libres)
 *  ──────────────────────────────────────────────────────────────────────
 *  Módulo SD  →  ESP32
 *  MOSI       →  GPIO23
 *  MISO       →  GPIO19
 *  SCK        →  GPIO18
 *  CS         →  GPIO5
 *  VCC        →  3V3 (o 5V según módulo, revisar regulador on-board)
 *  GND        →  GND
 * =============================================================================
 */

#ifndef TASK_SD_H
#define TASK_SD_H

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "driver/spi_common.h"
//#include "driver/sdspi_host.h"
#include "driver/sdmmc_host.h"
#include "driver/i2s_std.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

#include "IO_config.h"   /* SAMPLE_RATE_HZ, DMA_BUF_FRAMES */

static const char *SD_TAG = "SD_WAV";

/* ──────────────────────────────────────────────────────────────
 *  PINES SPI (VSPI por defecto del ESP32-WROOM-32D)
 * ────────────────────────────────────────────────────────────── */
#define SD_PIN_MOSI     GPIO_NUM_23
#define SD_PIN_MISO     GPIO_NUM_19
#define SD_PIN_SCLK     GPIO_NUM_18
#define SD_PIN_CS       GPIO_NUM_5
#define SD_SPI_HOST     SPI3_HOST   /* VSPI */
#define SD_MOUNT_POINT  "/sdcard"

/* ──────────────────────────────────────────────────────────────
 *  ESTADO COMPARTIDO CON Task_OLED.h Y Task_audio.h
 *  (sin "static": mismo criterio que instrumento_actual, etc.)
 * ────────────────────────────────────────────────────────────── */
#define SD_MAX_ARCHIVOS   8
#define SD_MAX_NOMBRE     32

volatile bool     sd_montada          = true;
volatile uint8_t  sd_wav_cantidad     = 0;
char               sd_wav_lista[SD_MAX_ARCHIVOS][SD_MAX_NOMBRE];

volatile int8_t   sd_test_cursor      = 0;   /* índice seleccionado en la lista */
volatile bool      sd_test_play_request = false;  /* pedido de reproducción (desde OLED)   */
volatile bool      sd_test_stop_request = false;  /* pedido de detención     (desde OLED)   */
volatile bool      sd_test_reproduciendo = false; /* estado real (lo actualiza audio_task)  */
volatile bool      sd_test_error_formato = false; /* .wav con formato no soportado          */

static sdmmc_card_t *sd_card_handle = NULL;

/* ──────────────────────────────────────────────────────────────
 *  ENCABEZADO WAV MÍNIMO YA PARSEADO
 * ────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t sample_rate;
    uint16_t num_canales;
    uint16_t bits_por_muestra;
    uint32_t data_offset;   /* offset absoluto del chunk "data" dentro del archivo */
    uint32_t data_size;     /* bytes de audio PCM */
} sd_wav_header_t;

static FILE            *sd_test_fp = NULL;
static sd_wav_header_t  sd_test_header;
static uint32_t          sd_test_bytes_restantes = 0;

/* ──────────────────────────────────────────────────────────────
 *  ESTADO PARA ARCHIVOS DE APRENDIZAJE (.apr)
 * ────────────────────────────────────────────────────────────── */
volatile uint8_t  sd_apr_cantidad     = 0;
char              sd_apr_lista[SD_MAX_ARCHIVOS][SD_MAX_NOMBRE];
volatile bool     sd_apr_en_carpeta   = false; /* true = /sdcard/APR, false = /sdcard */

/* Prototipos */
static void sd_escanear_wavs(void);
static void sd_escanear_aprs(void);

/* ──────────────────────────────────────────────────────────────
 *  MONTAJE DE LA SD (SPI + FATFS)
 * ────────────────────────────────────────────────────────────── */
static bool sd_init(void)
{
    esp_err_t ret;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(SD_TAG, "Fallo al inicializar bus SPI (%s)", esp_err_to_name(ret));
        return false;
    }

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = SD_PIN_CS;
    slot_cfg.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 512,
    };

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &sd_card_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(SD_TAG, "Fallo al montar SD (%s). Revisar cableado/formateo FAT32.", esp_err_to_name(ret));
        sd_montada = false;
        return false;
    }

    sd_montada = true;
    ESP_LOGI(SD_TAG, "SD montada en %s", SD_MOUNT_POINT);
    sdmmc_card_print_info(stdout, sd_card_handle);
    
    /* Auto-escanear apenas se monta la SD */
    sd_escanear_wavs();
    sd_escanear_aprs();

    return true;
}

/* ──────────────────────────────────────────────────────────────
 *  ESCANEO DINÁMICO DE ARCHIVOS .wav EN LA RAÍZ
 * ────────────────────────────────────────────────────────────── */
static void sd_escanear_wavs(void)
{
    sd_wav_cantidad = 0;
    if (!sd_montada) return;

    DIR *dir = opendir(SD_MOUNT_POINT);
    if (dir == NULL) {
        ESP_LOGE(SD_TAG, "No se pudo abrir directorio raiz de la SD");
        return;
    }

    struct dirent *entrada;
    while ((entrada = readdir(dir)) != NULL && sd_wav_cantidad < SD_MAX_ARCHIVOS) {
        size_t len = strlen(entrada->d_name);
        if (len < 5) continue; /* mínimo "x.wav" */

        const char *ext = entrada->d_name + (len - 4);
        if (strcasecmp(ext, ".wav") == 0) {
            strncpy(sd_wav_lista[sd_wav_cantidad], entrada->d_name, SD_MAX_NOMBRE - 1);
            sd_wav_lista[sd_wav_cantidad][SD_MAX_NOMBRE - 1] = '\0';
            sd_wav_cantidad++;
        }
    }
    closedir(dir);

    ESP_LOGI(SD_TAG, "Archivos .wav encontrados: %d", sd_wav_cantidad);
    for (int i = 0; i < sd_wav_cantidad; i++) ESP_LOGI(SD_TAG, "  [%d] %s", i, sd_wav_lista[i]);

    if (sd_test_cursor >= sd_wav_cantidad) sd_test_cursor = 0;
}

/* ──────────────────────────────────────────────────────────────
 *  ESCANEO DINÁMICO DE ARCHIVOS .apr
 * ────────────────────────────────────────────────────────────── */
static void sd_escanear_aprs(void)
{
    sd_apr_cantidad = 0;
    if (!sd_montada) return;

    /* Intentar abrir la carpeta APR, si falla, buscar en la raíz */
    DIR *dir = opendir(SD_MOUNT_POINT "/APR");
    if (dir != NULL) {
        sd_apr_en_carpeta = true;
    } else {
        dir = opendir(SD_MOUNT_POINT);
        sd_apr_en_carpeta = false;
    }

    if (dir == NULL) {
        ESP_LOGE(SD_TAG, "No se pudo abrir el directorio para buscar .apr");
        return;
    }

    struct dirent *entrada;
    while ((entrada = readdir(dir)) != NULL && sd_apr_cantidad < SD_MAX_ARCHIVOS) {
        size_t len = strlen(entrada->d_name);
        if (len < 5) continue; /* mínimo "x.apr" */

        const char *ext = entrada->d_name + (len - 4);
        if (strcasecmp(ext, ".apr") == 0) {
            strncpy(sd_apr_lista[sd_apr_cantidad], entrada->d_name, SD_MAX_NOMBRE - 1);
            sd_apr_lista[sd_apr_cantidad][SD_MAX_NOMBRE - 1] = '\0';
            sd_apr_cantidad++;
        }
    }
    closedir(dir);

    ESP_LOGI(SD_TAG, "Archivos .apr encontrados: %d (En carpeta APR: %s)", 
             sd_apr_cantidad, sd_apr_en_carpeta ? "Si" : "No");
}

/* ──────────────────────────────────────────────────────────────
 *  HELPER: OBTENER RUTA ABSOLUTA DEL ARCHIVO .apr
 * ────────────────────────────────────────────────────────────── */
static void sd_obtener_ruta_apr(uint8_t indice, char* ruta_salida, size_t max_len) {
    if (indice >= sd_apr_cantidad) {
        ruta_salida[0] = '\0';
        return;
    }
    if (sd_apr_en_carpeta) {
        snprintf(ruta_salida, max_len, "%s/APR/%s", SD_MOUNT_POINT, sd_apr_lista[indice]);
    } else {
        snprintf(ruta_salida, max_len, "%s/%s", SD_MOUNT_POINT, sd_apr_lista[indice]);
    }
}

/* ──────────────────────────────────────────────────────────────
 *  PARSER DE HEADER WAV (recorre chunks RIFF hasta hallar "fmt " y "data")
 * ────────────────────────────────────────────────────────────── */
static bool sd_leer_header_wav(FILE *fp, sd_wav_header_t *hdr)
{
    char id[4];
    uint32_t chunk_size;

    /* RIFF....WAVE */
    if (fread(id, 1, 4, fp) != 4 || memcmp(id, "RIFF", 4) != 0) return false;
    fseek(fp, 4, SEEK_CUR); /* tamaño total del RIFF, no lo necesitamos */
    if (fread(id, 1, 4, fp) != 4 || memcmp(id, "WAVE", 4) != 0) return false;

    bool fmt_ok = false, data_ok = false;

    while (!(fmt_ok && data_ok)) {
        if (fread(id, 1, 4, fp) != 4) break;
        if (fread(&chunk_size, 4, 1, fp) != 1) break;

        if (memcmp(id, "fmt ", 4) == 0) {
            uint16_t audio_format, num_canales, block_align, bits_por_muestra;
            uint32_t sample_rate, byte_rate;

            fread(&audio_format, 2, 1, fp);
            fread(&num_canales, 2, 1, fp);
            fread(&sample_rate, 4, 1, fp);
            fread(&byte_rate, 4, 1, fp);
            fread(&block_align, 2, 1, fp);
            fread(&bits_por_muestra, 2, 1, fp);

            hdr->num_canales = num_canales;
            hdr->sample_rate = sample_rate;
            hdr->bits_por_muestra = bits_por_muestra;

            /* Si el chunk fmt tiene más de 16 bytes (extensible), saltar el resto */
            if (chunk_size > 16) fseek(fp, chunk_size - 16, SEEK_CUR);
            fmt_ok = true;
        }
        else if (memcmp(id, "data", 4) == 0) {
            hdr->data_offset = (uint32_t)ftell(fp);
            hdr->data_size   = chunk_size;
            data_ok = true;
            /* No seek: nos quedamos posicionados al inicio de los datos */
        }
        else {
            /* Chunk desconocido (LIST, JUNK, etc.) -> saltar */
            fseek(fp, chunk_size + (chunk_size & 1), SEEK_CUR);
        }
    }

    return fmt_ok && data_ok;
}

/* ──────────────────────────────────────────────────────────────
 *  RECONFIGURACIÓN EN CALIENTE DEL CLOCK I2S
 *  (para que coincida con el sample rate real del .wav elegido)
 * ────────────────────────────────────────────────────────────── */
static void sd_reconfigurar_i2s(i2s_chan_handle_t tx_chan, uint32_t sample_rate)
{
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    clk_cfg.clk_src       = I2S_CLK_SRC_DEFAULT;
    clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    i2s_channel_disable(tx_chan);
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(tx_chan, &clk_cfg));
    i2s_channel_enable(tx_chan);
}

/* ──────────────────────────────────────────────────────────────
 *  INICIAR REPRODUCCIÓN DEL ARCHIVO SELECCIONADO
 *  Llamada desde audio_playback_task al detectar sd_test_play_request.
 * ────────────────────────────────────────────────────────────── */
static bool sd_test_iniciar(i2s_chan_handle_t tx_chan)
{
    if (!sd_montada || sd_wav_cantidad == 0) return false;

    char ruta[SD_MAX_NOMBRE + 16];
    snprintf(ruta, sizeof(ruta), "%s/%s", SD_MOUNT_POINT, sd_wav_lista[sd_test_cursor]);

    if (sd_test_fp != NULL) { fclose(sd_test_fp); sd_test_fp = NULL; }

    sd_test_fp = fopen(ruta, "rb");
    if (sd_test_fp == NULL) {
        ESP_LOGE(SD_TAG, "No se pudo abrir %s", ruta);
        return false;
    }

    if (!sd_leer_header_wav(sd_test_fp, &sd_test_header)) {
        ESP_LOGE(SD_TAG, "Header WAV invalido en %s", ruta);
        fclose(sd_test_fp); sd_test_fp = NULL;
        sd_test_error_formato = true;
        return false;
    }

    /* Solo soportamos PCM 16 bits estéreo/mono en esta prueba rápida */
    if (sd_test_header.bits_por_muestra != 16 ||
        (sd_test_header.num_canales != 1 && sd_test_header.num_canales != 2)) {
        ESP_LOGE(SD_TAG, "Formato no soportado: %d bits, %d canales",
                 sd_test_header.bits_por_muestra, sd_test_header.num_canales);
        fclose(sd_test_fp); sd_test_fp = NULL;
        sd_test_error_formato = true;
        return false;
    }

    sd_test_error_formato = false;
    sd_test_bytes_restantes = sd_test_header.data_size;

    ESP_LOGI(SD_TAG, "Reproduciendo %s: %lu Hz, %d ch, %d bit",
             sd_wav_lista[sd_test_cursor],
             (unsigned long)sd_test_header.sample_rate,
             sd_test_header.num_canales, sd_test_header.bits_por_muestra);

    sd_reconfigurar_i2s(tx_chan, sd_test_header.sample_rate);

    sd_test_reproduciendo = true;
    return true;
}

/* Cierra el archivo y devuelve el I2S a la frecuencia estándar del sintetizador */
static void sd_test_detener(i2s_chan_handle_t tx_chan)
{
    if (sd_test_fp != NULL) { fclose(sd_test_fp); sd_test_fp = NULL; }
    sd_test_reproduciendo = false;
    sd_test_bytes_restantes = 0;
    sd_reconfigurar_i2s(tx_chan, SAMPLE_RATE_HZ);
}

/* ──────────────────────────────────────────────────────────────
 *  LLENAR UN BUFFER DE SALIDA CON AUDIO CRUDO DEL ARCHIVO
 *  Devuelve la cantidad de bytes efectivamente escritos en 'destino'.
 *  Si el archivo es mono, duplica cada muestra a L/R.
 *  Si llega a EOF, rellena el resto con silencio y avisa (return < len_bytes).
 * ────────────────────────────────────────────────────────────── */
static size_t sd_test_llenar_buffer(int16_t *destino, size_t frames)
{
    if (sd_test_fp == NULL || !sd_test_reproduciendo) {
        memset(destino, 0, frames * 2 * sizeof(int16_t));
        return 0;
    }

    size_t frames_leidos = 0;

    if (sd_test_header.num_canales == 2) {
        /* Match directo: WAV estéreo 16 bit == layout de i2s_tx_buf */
        size_t bytes_pedidos = frames * 2 * sizeof(int16_t);
        size_t bytes_a_leer  = (bytes_pedidos < sd_test_bytes_restantes)
                                ? bytes_pedidos : sd_test_bytes_restantes;

        size_t bytes_leidos = fread(destino, 1, bytes_a_leer, sd_test_fp);
        sd_test_bytes_restantes -= bytes_leidos;
        frames_leidos = bytes_leidos / (2 * sizeof(int16_t));

        if (bytes_leidos < bytes_pedidos) {
            memset(((uint8_t *)destino) + bytes_leidos, 0, bytes_pedidos - bytes_leidos);
        }
    } else {
        /* Mono: leer a un scratch y duplicar a L/R */
        static int16_t mono_scratch[DMA_BUF_FRAMES];
        size_t bytes_pedidos = frames * sizeof(int16_t);
        size_t bytes_a_leer  = (bytes_pedidos < sd_test_bytes_restantes)
                                ? bytes_pedidos : sd_test_bytes_restantes;

        size_t bytes_leidos = fread(mono_scratch, 1, bytes_a_leer, sd_test_fp);
        sd_test_bytes_restantes -= bytes_leidos;
        frames_leidos = bytes_leidos / sizeof(int16_t);

        for (size_t i = 0; i < frames; i++) {
            int16_t m = (i < frames_leidos) ? mono_scratch[i] : 0;
            destino[i * 2]     = m;
            destino[i * 2 + 1] = m;
        }
    }

    if (sd_test_bytes_restantes == 0) {
        /* Fin de archivo: cerrar, la tarea de audio detecta reproduciendo=false
           en la próxima vuelta y restaura el clock estándar */
        fclose(sd_test_fp);
        sd_test_fp = NULL;
        sd_test_reproduciendo = false;
    }

    return frames_leidos;
}

#endif /* TASK_SD_H */
