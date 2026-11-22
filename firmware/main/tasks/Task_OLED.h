/* =============================================================================
 *  Task_OLED.h  —  Motor Gráfico HMI, Menú Multinivel y Modo Prueba
 * =============================================================================
 */

#ifndef TASK_OLED_H
#define TASK_OLED_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "u8g2.h"
#include "Task_SD.h"
#include "Instrumentos.h"

extern QueueHandle_t cola_oled;
extern QueueHandle_t cola_de_datos_PCF8574_to_OLED;

static u8g2_t oled_u8g2;

/* Variables importadas del PCF8574 para control de LEDs */
extern volatile bool pcf_led_menu;
extern volatile bool pcf_led_res1;
extern volatile bool pcf_led_res2;
extern volatile bool pcf_led_res3;

/* Variables Globales de Estado del Sintetizador */
extern volatile uint8_t instrumento_actual;
extern volatile uint8_t indice_escala_base; 
volatile uint8_t modo_funcionamiento = 1; /* 0=Aprendizaje, 1=Sintesis, 2=Prueba */
volatile bool sustain_activado = true;

typedef enum {
    METRONOMO_OFF = 0,
    METRONOMO_3_4,
    METRONOMO_4_4,
    METRONOMO_5_4,
    METRONOMO_6_8,
    METRONOMO_7_8
} CompasMetronomo_t;

volatile CompasMetronomo_t metronomo_compas = METRONOMO_OFF;
volatile uint16_t metronomo_bpm = 100;
static bool metronomo_editando_bpm = false;

static const char* metronomo_texto(CompasMetronomo_t c) {
    switch (c) {
        case METRONOMO_3_4: return "3/4";
        case METRONOMO_4_4: return "4/4";
        case METRONOMO_5_4: return "5/4";
        case METRONOMO_6_8: return "6/8";
        case METRONOMO_7_8: return "7/8";
        default:            return "OFF";
    }
}

/* Variables Globales de Estado de la FSM para modo aprendizaje */
extern volatile uint16_t ap_paso_actual;
extern volatile uint16_t ap_paso_total;
extern volatile uint8_t  ap_estado_fsm;

/* Comandos PCF */
#define EV_PCF_ENTER_SHORT   1
#define EV_PCF_UP            2
#define EV_PCF_DOWN          3
#define EV_PCF_BACK          4
#define EV_PCF_ENTER_LONG    5

/* Variables externas de SD y Aprendizaje */
extern volatile uint8_t sd_apr_cantidad;
extern char sd_apr_lista[8][32]; /* SD_MAX_ARCHIVOS y SD_MAX_NOMBRE */
extern volatile uint8_t ap_fuente_actual;

/* Variable global para el índice seleccionado en la SD */
volatile uint8_t ap_sd_indice = 0; 

/* Actualización de Estados de la Pantalla */
typedef enum {
    PANTALLA_SPLASH,
    PANTALLA_SINTESIS,
    PANTALLA_APRENDIZAJE,
    PANTALLA_PRUEBA,
    MENU_MAIN,
    MENU_MODO,
    MENU_INST,
    MENU_NOTA_INICIO,
    MENU_OTROS,
    MENU_CANCION
} EstadoPantalla_t;

static EstadoPantalla_t estado_actual = PANTALLA_SPLASH;
static EstadoPantalla_t estado_anterior = PANTALLA_SINTESIS; 

static int cursor_idx = 0;   
static int menu_scroll = 0;  

/* Variables exclusivas para el Modo de Prueba */
/* Etapas del Modo Prueba (las usan también Task_TiraLED.h y Task_audio.h) */
#define PRUEBA_INICIO    0
#define PRUEBA_LEDS      1   /* solo tira LED                */
#define PRUEBA_AUDIO     2   /* solo tono de 440 Hz          */
#define PRUEBA_PCF       3
#define PRUEBA_TECLADO   4
#define PRUEBA_SD        5
#define PRUEBA_ULTIMA    PRUEBA_SD

volatile uint8_t subestado_prueba = PRUEBA_INICIO;
static uint32_t ultima_tecla_test = 0;
static uint32_t ultimo_pcf_test = 0;


/* ==============================================================
 *  CALLBACKS I2C Y U8G2 (MANTENIDOS EXACTAMENTE IGUAL)
 * ============================================================== */
static inline void i2c_sda(uint8_t val) { gpio_set_level(OLED_PIN_SDA, val); }
static inline void i2c_scl(uint8_t val) { gpio_set_level(OLED_PIN_SCK, val); }
static inline void i2c_delay(void)      { esp_rom_delay_us(5); }  

static void i2c_start(void) {
    i2c_sda(1); i2c_scl(1); i2c_delay();
    i2c_sda(0);             i2c_delay();   
    i2c_scl(0);             i2c_delay();
}

static void i2c_stop(void) {
    i2c_sda(0); i2c_scl(0); i2c_delay();
    i2c_scl(1);              i2c_delay();  
    i2c_sda(1);              i2c_delay();  
}

static void i2c_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        i2c_sda((byte >> i) & 1);
        i2c_delay();
        i2c_scl(1); i2c_delay();
        i2c_scl(0); i2c_delay();
    }
    i2c_sda(1);
    i2c_delay();
    i2c_scl(1); i2c_delay();
    i2c_scl(0); i2c_delay();
}

static uint8_t u8x8_byte_esp32_sw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
            gpio_reset_pin(OLED_PIN_SDA); gpio_reset_pin(OLED_PIN_SCK);
            gpio_set_direction(OLED_PIN_SDA, GPIO_MODE_OUTPUT_OD);
            gpio_set_direction(OLED_PIN_SCK, GPIO_MODE_OUTPUT_OD);
            gpio_set_pull_mode(OLED_PIN_SDA, GPIO_PULLUP_ONLY);
            gpio_set_pull_mode(OLED_PIN_SCK, GPIO_PULLUP_ONLY);
            gpio_set_level(OLED_PIN_SDA, 1); gpio_set_level(OLED_PIN_SCK, 1);
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            i2c_start(); i2c_write_byte(u8x8_GetI2CAddress(u8x8)); break;
        case U8X8_MSG_BYTE_SEND: {
            uint8_t *data = (uint8_t *)arg_ptr;
            while (arg_int--) i2c_write_byte(*data++);
            break;
        }
        case U8X8_MSG_BYTE_END_TRANSFER:
            i2c_stop(); break;
        default: return 0;
    }
    return 1;
}

static uint8_t u8x8_esp32_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_DELAY_MILLI:   vTaskDelay(pdMS_TO_TICKS(arg_int ? arg_int : 1)); break;
        case U8X8_MSG_DELAY_10MICRO: esp_rom_delay_us(10U * arg_int); break;
        case U8X8_MSG_DELAY_100NANO: esp_rom_delay_us(1); break;
        default: return 0;
    }
    return 1;
}

static inline void oled_init(void) {
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&oled_u8g2, U8G2_R0, u8x8_byte_esp32_sw_i2c, u8x8_esp32_delay);
    u8g2_InitDisplay(&oled_u8g2);
    u8g2_SetPowerSave(&oled_u8g2, 0);
}

/* ──────────────────────────────────────────────────────────────
 *  TEXTO SEGURO PARA LA OLED
 * ────────────────────────────────────────────────────────────── */
static void oled_texto_corto(const char *src, char *dst, size_t max_vis)
{
    char limpio[48];
    size_t n = 0;
    for (size_t k = 0; src[k] != '\0' && n < sizeof(limpio) - 1; k++) {
        unsigned char c = (unsigned char)src[k];
        if (c >= 32 && c <= 126)        limpio[n++] = (char)c;
        else if ((c & 0xC0) != 0x80)    limpio[n++] = '?';
    }
    limpio[n] = '\0';

    size_t lim = (n > max_vis) ? max_vis - 1 : n;
    memcpy(dst, limpio, lim);
    if (n > max_vis) dst[lim++] = '~';
    dst[lim] = '\0';
}

static void oled_nombre_archivo(const char *src, char *dst, size_t max_vis)
{
    char tmp[48];
    size_t n = strlen(src);
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    memcpy(tmp, src, n);
    tmp[n] = '\0';
    char *punto = strrchr(tmp, '.');
    if (punto != NULL && punto != tmp && strlen(punto) <= 5) *punto = '\0';
    oled_texto_corto(tmp, dst, max_vis);
}

static uint8_t oled_ventana_lista(int cursor, int total, int filas, uint8_t scroll)
{
    if (total <= filas) return 0;
    if (cursor < 0) cursor = 0;
    if (cursor >= total) cursor = total - 1;
    if (cursor >= scroll + filas) scroll = (uint8_t)(cursor - (filas - 1));
    if (cursor < scroll)          scroll = (uint8_t)cursor;
    if (scroll > total - filas)   scroll = (uint8_t)(total - filas);
    return scroll;
}
static uint8_t wav_scroll = 0;

static void dibujar_menu(const char* titulo, const char** opciones, int num_opciones, int activo_idx) {
    u8g2_ClearBuffer(&oled_u8g2);
    u8g2_SetFont(&oled_u8g2, u8g2_font_6x10_tr);

    u8g2_DrawStr(&oled_u8g2, 0, 10, titulo);
    u8g2_DrawHLine(&oled_u8g2, 0, 12, 128);

    if (cursor_idx >= menu_scroll + 4) menu_scroll = cursor_idx - 3;
    if (cursor_idx < menu_scroll) menu_scroll = cursor_idx;

    for (int i = 0; i < 4; i++) {
        int opt_idx = menu_scroll + i;
        if (opt_idx >= num_opciones) break;

        int y_pos = 25 + (i * 12);
        
        if (opt_idx == activo_idx) u8g2_DrawStr(&oled_u8g2, 0, y_pos, "->");

        if (opt_idx == cursor_idx) {
            u8g2_SetDrawColor(&oled_u8g2, 1);
            u8g2_DrawBox(&oled_u8g2, 12, y_pos - 9, 116, 11);
            u8g2_SetDrawColor(&oled_u8g2, 0); 
        } else {
            u8g2_SetDrawColor(&oled_u8g2, 1); 
        }
        
        u8g2_DrawStr(&oled_u8g2, 14, y_pos, opciones[opt_idx]);
        u8g2_SetDrawColor(&oled_u8g2, 1); 
    }
    u8g2_SendBuffer(&oled_u8g2);
}

/* ──────────────────────────────────────────────────────────────
 *  PANTALLAS PRINCIPALES Y MODO PRUEBA
 * ────────────────────────────────────────────────────────────── */
static void dibujar_pantalla_activa() {
    u8g2_ClearBuffer(&oled_u8g2);
    u8g2_SetFont(&oled_u8g2, u8g2_font_6x10_tr);

    switch(estado_actual) {
        case PANTALLA_SPLASH:
            u8g2_DrawStr(&oled_u8g2, 10, 20, "Proyecto de Grado");
            u8g2_DrawStr(&oled_u8g2, 0, 40, "Teclado Orientado al");
            u8g2_DrawStr(&oled_u8g2, 0, 50, "Aprendizaje de Musica");
            break;

        case PANTALLA_SINTESIS:
            u8g2_SetFont(&oled_u8g2, u8g2_font_ncenB08_tr);
            u8g2_DrawStr(&oled_u8g2, 5, 20, "Modo Sintetizador");
            u8g2_SetFont(&oled_u8g2, u8g2_font_6x10_tr);
            u8g2_DrawStr(&oled_u8g2, 5, 45, "Instrumento:");
            /* El nombre sale de la tabla de instrumentos: sigue siendo correcto
               aunque se agreguen o quiten instrumentos en Instrumentos.h */
            u8g2_DrawStr(&oled_u8g2, 5, 55,
                         instrumentos_tabla[instrumento_actual < INSTR_CANTIDAD ? instrumento_actual : 0].nombre);
            break;

        case PANTALLA_APRENDIZAJE:
            u8g2_DrawStr(&oled_u8g2, 0, 15, "Modo Aprendizaje");
            u8g2_DrawHLine(&oled_u8g2, 0, 18, 128);
            
            char buf_progreso[40];
            if (ap_estado_fsm == 0) {
                u8g2_DrawStr(&oled_u8g2, 0, 35, "Cargando cancion...");
            } else if (ap_estado_fsm == 3) {
                u8g2_DrawStr(&oled_u8g2, 0, 30, "Practica Finalizada!");
                sprintf(buf_progreso, "Pasos: %d", ap_paso_actual);
                u8g2_DrawStr(&oled_u8g2, 0, 45, buf_progreso);
                u8g2_DrawStr(&oled_u8g2, 0, 60, "[MENU] Salir");
            } else {
                char buf_nombre[40];
                if (ap_fuente_actual > 0 && ap_fuente_actual <= 4) {
                    const char* nombres_flash[] = {"Feliz Cumple", "Estrellita", "Oda Alegria", "Para Elisa"};
                    sprintf(buf_nombre, "F: %s", nombres_flash[ap_fuente_actual - 1]);
                } else {
                    sprintf(buf_nombre, "SD: %s", sd_apr_lista[ap_sd_indice]);
                }
                u8g2_DrawStr(&oled_u8g2, 0, 32, buf_nombre);
                
                sprintf(buf_progreso, "Progreso: %d", ap_paso_actual);
                u8g2_DrawStr(&oled_u8g2, 0, 48, buf_progreso);
                if (ap_estado_fsm == 2) {
                    u8g2_DrawStr(&oled_u8g2, 0, 62, "<- Toca la tecla ->");
                }
            }
            break;

        case PANTALLA_PRUEBA:
            if (subestado_prueba == PRUEBA_SD) u8g2_DrawStr(&oled_u8g2, 0, 10, "Prueba 5: SD / WAV");
            else                       u8g2_DrawStr(&oled_u8g2, 0, 10, "-- HW & SW TEST --");
            u8g2_DrawHLine(&oled_u8g2, 0, 12, 128);
            
            if (subestado_prueba == PRUEBA_INICIO) {
                u8g2_DrawStr(&oled_u8g2, 5, 30, "Presione [ENT]");
                u8g2_DrawStr(&oled_u8g2, 5, 45, "para iniciar sec.");
            } else if (subestado_prueba == PRUEBA_LEDS) {
                u8g2_DrawStr(&oled_u8g2, 5, 30, "Prueba 1: Tira LED");
                u8g2_DrawStr(&oled_u8g2, 5, 45, "Rojo/Verde/Azul 1 s");
                u8g2_DrawStr(&oled_u8g2, 5, 60, "[ENT] Siguiente");
            } else if (subestado_prueba == PRUEBA_AUDIO) {
                u8g2_DrawStr(&oled_u8g2, 5, 30, "Prueba 2: Audio");
                u8g2_DrawStr(&oled_u8g2, 5, 45, "Tono 440 Hz");
                u8g2_DrawStr(&oled_u8g2, 5, 60, "[ENT] Siguiente");
            } else if (subestado_prueba == PRUEBA_PCF) {
                char buf[30];
                u8g2_DrawStr(&oled_u8g2, 5, 30, "Prueba 3: PCF & LEDs");
                sprintf(buf, "Pulsador ID: %lu", (unsigned long)ultimo_pcf_test);
                u8g2_DrawStr(&oled_u8g2, 5, 45, buf);
                u8g2_DrawStr(&oled_u8g2, 5, 60, "[ENT] Siguiente");
            } else if (subestado_prueba == PRUEBA_TECLADO) {
                char buf[30];
                u8g2_DrawStr(&oled_u8g2, 5, 30, "Prueba 4: Teclado");
                sprintf(buf, "Tecla ID: %lu", (unsigned long)ultima_tecla_test);
                u8g2_DrawStr(&oled_u8g2, 5, 45, buf);
                u8g2_DrawStr(&oled_u8g2, 5, 60, "[ENT] Siguiente");
            } else if (subestado_prueba == PRUEBA_SD) {
                if (!sd_montada) {
                    u8g2_DrawStr(&oled_u8g2, 5, 40, "SD no detectada");
                    u8g2_DrawStr(&oled_u8g2, 5, 52, "Revisar cableado");
                } else if (sd_wav_cantidad == 0) {
                    u8g2_DrawStr(&oled_u8g2, 5, 40, "Sin archivos .wav");
                } else if (sd_test_error_formato) {
                    u8g2_DrawStr(&oled_u8g2, 5, 40, "Formato no soportado");
                } else {
                    const int filas = 3;
                    wav_scroll = oled_ventana_lista(sd_test_cursor, sd_wav_cantidad, filas, wav_scroll);

                    for (int f = 0; f < filas; f++) {
                        int i = wav_scroll + f;
                        if (i >= sd_wav_cantidad) break;
                        int y = 24 + (f * 12);
                        char nombre[24];
                        char linea[28];
                        oled_nombre_archivo(sd_wav_lista[i], nombre, 18);
                        snprintf(linea, sizeof(linea), "%s%s",
                                 (i == sd_test_cursor) ? "> " : "  ",
                                 nombre);
                        u8g2_DrawStr(&oled_u8g2, 5, y, linea);
                    }

                    if (sd_wav_cantidad > filas) {
                        const int y0 = 14, alto = 38;
                        int th = (alto * filas) / sd_wav_cantidad;
                        if (th < 4) th = 4;
                        int ty = y0 + ((alto - th) * wav_scroll) / (sd_wav_cantidad - filas);
                        u8g2_DrawVLine(&oled_u8g2, 127, y0, alto);
                        u8g2_DrawBox(&oled_u8g2, 125, ty, 3, th);
                    }
                    u8g2_DrawStr(&oled_u8g2, 0, 62,
                        sd_test_reproduciendo ? "Reproduciendo [BACK]"
                                               : "[ENT]Play [BACK]Salir");
                }
            }
            break;
            
        default: break; 
    }
    if (estado_actual < MENU_MAIN) u8g2_SendBuffer(&oled_u8g2);
}

static uint8_t     nota_lista[INSTR_MAX_INICIOS];
static char        nota_txt[INSTR_MAX_INICIOS][16];
static const char *nota_ptr[INSTR_MAX_INICIOS];
static uint8_t     nota_n = 0;

static int nota_menu_preparar(void)
{
    nota_n = instr_notas_inicio(instrumento_actual, nota_lista, INSTR_MAX_INICIOS);
    int vigente = 0;
    for (int i = 0; i < nota_n; i++) {
        char nom[8];
        instr_nombre_nota(nota_lista[i], nom, sizeof(nom));
        snprintf(nota_txt[i], sizeof(nota_txt[i]), "%s  (%d)", nom, nota_lista[i]);
        nota_ptr[i] = nota_txt[i];
        if (nota_lista[i] == indice_escala_base) vigente = i;
    }
    return vigente;
}

/* ──────────────────────────────────────────────────────────────
 *  MÁQUINA DE ESTADOS (TAREA PRINCIPAL)
 * ────────────────────────────────────────────────────────────── */
static void tarea_oled(void *arg)
{
    const char* m_main[] = {"Modo Funcionamiento", "Instrumento", "Nota Inicial", "Otros"};
    const char* m_modo[] = {"Aprendizaje", "Sintetizador", "Prueba Hardware"};
    /* Mismo orden que instrumentos_tabla en Instrumentos.h */
    const char* m_inst[] = {"Salamander Piano", "Guitarra"};
    /* Ondas básicas deshabilitadas por ahora:
    const char* m_inst[] = {"Salamander Piano", "Guitarra", "Senoidal", "Cuadrada", "Triangular", "Sierra"}; */
    char txt_sustain[30];

    estado_actual = PANTALLA_SPLASH;
    dibujar_pantalla_activa();
    vTaskDelay(pdMS_TO_TICKS(3000));
    estado_actual = PANTALLA_SINTESIS;
    dibujar_pantalla_activa();

    uint32_t comando_pcf;
    uint32_t dato_teclado;

    uint16_t ap_paso_last = 0xFFFF;
    uint8_t  ap_estado_last = 0xFF;

    for (;;) {
        bool actualizar_pantalla = false;

        /* FASE DE CÁLCULO: Procesar Entradas del PCF8574 */
        if (xQueueReceive(cola_de_datos_PCF8574_to_OLED, &comando_pcf, pdMS_TO_TICKS(50)) == pdTRUE) {
            actualizar_pantalla = true;
            
            /* --- INGRESO Y SALIDA DEL MENÚ (Flanco Pulsación Larga) --- */
            if (comando_pcf == EV_PCF_ENTER_LONG) {
                metronomo_editando_bpm = false;
                if (estado_actual < MENU_MAIN) {
                    estado_anterior = estado_actual;
                    estado_actual = MENU_MAIN;
                    cursor_idx = 0; menu_scroll = 0;
                    pcf_led_menu = true; 
                } else {
                    pcf_led_menu = false;
                    if (modo_funcionamiento == 0) estado_actual = PANTALLA_APRENDIZAJE;
                    else if (modo_funcionamiento == 1) estado_actual = PANTALLA_SINTESIS;
                    else {
                        estado_actual = PANTALLA_PRUEBA;
                        subestado_prueba = 0; 
                    }
                }
            }
            /* --- NAVEGACIÓN DENTRO DEL MENÚ --- */
            else if (estado_actual >= MENU_MAIN) {
                if (metronomo_editando_bpm) {
                    if (comando_pcf == EV_PCF_UP   && metronomo_bpm < 208) metronomo_bpm += 4;
                    if (comando_pcf == EV_PCF_DOWN && metronomo_bpm > 40)  metronomo_bpm -= 4;
                    if (comando_pcf == EV_PCF_BACK || comando_pcf == EV_PCF_ENTER_SHORT) {
                        metronomo_editando_bpm = false;
                    }
                    actualizar_pantalla = true;
                }
                else {
                    if (comando_pcf == EV_PCF_UP && cursor_idx > 0) cursor_idx--;
                    if (comando_pcf == EV_PCF_BACK) {
                        estado_actual = MENU_MAIN;
                        cursor_idx = 0; menu_scroll = 0;
                    }

                    switch (estado_actual) {
                        case MENU_MAIN:
                            if (comando_pcf == EV_PCF_DOWN && cursor_idx < 3) cursor_idx++;
                            if (comando_pcf == EV_PCF_ENTER_SHORT) {
                                if (cursor_idx == 0) estado_actual = MENU_MODO;
                                if (cursor_idx == 1) estado_actual = MENU_INST;
                                if (cursor_idx == 2) estado_actual = MENU_NOTA_INICIO;
                                if (cursor_idx == 3) estado_actual = MENU_OTROS;
                                cursor_idx = 0; menu_scroll = 0;
                                if (estado_actual == MENU_NOTA_INICIO) cursor_idx = nota_menu_preparar();
                            }
                            break;

                        case MENU_MODO:
                            if (comando_pcf == EV_PCF_DOWN && cursor_idx < 2) cursor_idx++;
                            if (comando_pcf == EV_PCF_ENTER_SHORT) {
                                if (cursor_idx == 0) {
                                    estado_actual = MENU_CANCION;
                                    cursor_idx = 0; menu_scroll = 0;
                                } else {
                                    modo_funcionamiento = cursor_idx;
                                    estado_actual = (modo_funcionamiento == 1) ? PANTALLA_SINTESIS : PANTALLA_PRUEBA;
                                    if(modo_funcionamiento == 2) subestado_prueba = 0;
                                    pcf_led_menu = false;
                                }
                            }
                            break;

                        case MENU_CANCION: {
                            int total_canciones = 4 + sd_apr_cantidad;
                            if (comando_pcf == EV_PCF_DOWN && cursor_idx < total_canciones - 1) cursor_idx++;
                            if (comando_pcf == EV_PCF_ENTER_SHORT) {
                                if (cursor_idx < 4) {                
                                    ap_fuente_actual = cursor_idx + 1; /* 1 a 4 = Flash */
                                } else {
                                    ap_fuente_actual = 0;              /* 0 = SD */
                                    ap_sd_indice = cursor_idx - 4;   
                                }
                                modo_funcionamiento = 0; 
                                ap_estado_fsm = 0;       
                                estado_actual = PANTALLA_APRENDIZAJE;
                                pcf_led_menu = false;
                            }
                            break;
                        }

                        case MENU_INST:
                            if (comando_pcf == EV_PCF_DOWN && cursor_idx < (INSTR_CANTIDAD - 1)) cursor_idx++; 
                            if (comando_pcf == EV_PCF_ENTER_SHORT) {
                                indice_escala_base = instr_inicio_mas_cercano((uint8_t)cursor_idx, indice_escala_base);
                                instrumento_actual = cursor_idx;
                            }
                            break;

                        case MENU_NOTA_INICIO:
                            if (comando_pcf == EV_PCF_DOWN && cursor_idx < nota_n - 1) cursor_idx++;
                            if (comando_pcf == EV_PCF_ENTER_SHORT && cursor_idx < nota_n) indice_escala_base = nota_lista[cursor_idx];
                            break;

                        case MENU_OTROS:
                            if (comando_pcf == EV_PCF_DOWN && cursor_idx < 2) cursor_idx++;
                            if (comando_pcf == EV_PCF_ENTER_SHORT) {
                                if (cursor_idx == 0) {
                                    sustain_activado = !sustain_activado;
                                } else if (cursor_idx == 1) {
                                    metronomo_compas = (metronomo_compas == METRONOMO_7_8)
                                    ? METRONOMO_OFF
                                    : (CompasMetronomo_t)(metronomo_compas + 1);
                                } else if (cursor_idx == 2) {
                                    metronomo_editando_bpm = true;
                                }
                            }
                            break;
                            
                        default: break;
                    }
                }
            } 
            /* --- SECUENCIA MANUAL MODO DE PRUEBA --- */
            else if (estado_actual == PANTALLA_PRUEBA) {

                if (subestado_prueba == PRUEBA_SD) {
                    if (comando_pcf == EV_PCF_UP && sd_test_cursor > 0) {
                        sd_test_cursor--;
                    }
                    if (comando_pcf == EV_PCF_DOWN && sd_test_cursor < (int8_t)sd_wav_cantidad - 1) {
                        sd_test_cursor++;
                    }
                    if (comando_pcf == EV_PCF_ENTER_SHORT && sd_wav_cantidad > 0) {
                        sd_test_play_request = true;
                    }
                    if (comando_pcf == EV_PCF_BACK) {
                        sd_test_stop_request = true;
                        subestado_prueba = PRUEBA_INICIO;
                    }
                } else {
                    if (comando_pcf == EV_PCF_ENTER_SHORT) {
                        subestado_prueba++;
                        if (subestado_prueba > PRUEBA_ULTIMA) subestado_prueba = PRUEBA_INICIO;

                        ultimo_pcf_test = 0;
                        ultima_tecla_test = 0;
                        pcf_led_res1 = false; pcf_led_res2 = false; pcf_led_res3 = false;

                        if (subestado_prueba == PRUEBA_SD) {
                            sd_escanear_wavs();
                            wav_scroll = 0;
                        }
                    }

                    if (subestado_prueba == PRUEBA_PCF) {
                        if (comando_pcf == EV_PCF_UP)   ultimo_pcf_test = 1;
                        if (comando_pcf == EV_PCF_DOWN) ultimo_pcf_test = 2;
                        if (comando_pcf == EV_PCF_BACK) ultimo_pcf_test = 4;

                        pcf_led_res1 = (ultimo_pcf_test == 1);
                        pcf_led_res2 = (ultimo_pcf_test == 2);
                        pcf_led_res3 = (ultimo_pcf_test == 4);
                    }
                }
            }
        }
        
        /* FASE DE CÁLCULO: Procesar Entradas de la Matriz */
        if (xQueueReceive(cola_oled, &dato_teclado, 0) == pdTRUE) {
            if (estado_actual == PANTALLA_PRUEBA && subestado_prueba == PRUEBA_TECLADO) {
                ultima_tecla_test = dato_teclado;
                actualizar_pantalla = true;
            }
        }

        /* Detección automática de progreso en Aprendizaje */
        if (estado_actual == PANTALLA_APRENDIZAJE) {
            if (ap_paso_actual != ap_paso_last || ap_estado_fsm != ap_estado_last) {
                ap_paso_last = ap_paso_actual;
                ap_estado_last = ap_estado_fsm;
                actualizar_pantalla = true;
            }
        }

        /* FASE DE RENDERIZADO: Dibujar solo si hubo algún cambio */
        if (actualizar_pantalla) {
            if (estado_actual < MENU_MAIN) {
                dibujar_pantalla_activa();
            } else {
                switch (estado_actual) {
                    case MENU_MAIN: 
                        dibujar_menu("MENU PRINCIPAL", m_main, 4, -1); 
                        break;
                    case MENU_MODO: 
                        dibujar_menu("MODO OPERACION", m_modo, 3, modo_funcionamiento); 
                        break;
                    case MENU_CANCION: {
                        uint8_t canciones_max = 20;
                        const char* m_canciones[canciones_max];
                        m_canciones[0] = "Flash: Feliz Cumple";
                        m_canciones[1] = "Flash: Estrellita";
                        m_canciones[2] = "Flash: Oda Alegria";
                        m_canciones[3] = "Flash: Para Elisa";
                        static char nom_sd[SD_MAX_ARCHIVOS][20];
                        for(int i = 0; i < sd_apr_cantidad; i++){
                            oled_nombre_archivo(sd_apr_lista[i], nom_sd[i], 18);
                            m_canciones[4+i] = nom_sd[i];
                        }
                        dibujar_menu("SELECCIONAR CANCION", m_canciones, 4 + sd_apr_cantidad, -1);
                        break;
                    }
                    case MENU_INST: 
                        dibujar_menu("INSTRUMENTO", m_inst, INSTR_CANTIDAD, instrumento_actual); 
                        break;
                    case MENU_NOTA_INICIO: {
                        int nota_activa = -1;
                        for (int i = 0; i < nota_n; i++) if (nota_lista[i] == indice_escala_base) nota_activa = i;
                        dibujar_menu("NOTA INICIAL", nota_ptr, nota_n, nota_activa); 
                        break;
                    }
                    case MENU_OTROS:
                        char txt_compas[24];
                        char txt_bpm[24];
                        sprintf(txt_sustain, "Sustain: %s", sustain_activado ? "ON" : "OFF");
                        sprintf(txt_compas, "Compas: %s", metronomo_texto(metronomo_compas));
                        sprintf(txt_bpm, metronomo_editando_bpm ? "BPM: < %d >" : "BPM: %d",
                        metronomo_bpm);
                        const char* m_otros[] = {txt_sustain, txt_compas, txt_bpm};
                        dibujar_menu("OTROS", m_otros, 3, -1);
                        break;
                    default: break;
                }
            }
        }
    }
}

#endif /* TASK_OLED_H */