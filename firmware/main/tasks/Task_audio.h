/* =============================================================================
 * Task_audio.h  —  Motor de Síntesis DDS Polifónico en RAM
 *
 * ESP-IDF v5.x | PCM5102A DAC via I2S
 *
 * Polifonía limitada a MAX_VOCES (IO_config.h): la carga del núcleo 1 queda
 * acotada aunque se aprieten todas las teclas. El metrónomo se mezcla aparte
 * y no ocupa voces, así que nunca se roba.
 * =============================================================================
 */

#ifndef TASK_AUDIO_H
#define TASK_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>          /* Para memcpy */
#include <math.h>            /* Para powf */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#include "IO_config.h"       
#include "Instrumentos.h"    /* Define ZonaWavetable_t e incluye los bancos (piano y guitarra) */
#include "Task_SD.h"
#include "Inicio.h"
#include "Prueba_estres.h"   /* PRUEBA ESTRES: medición del tiempo de cálculo */

/* Variables Externas importadas desde Task_OLED.h */
extern volatile uint8_t modo_funcionamiento;
extern volatile uint8_t subestado_prueba;
extern volatile bool sustain_activado;
extern volatile CompasMetronomo_t metronomo_compas;
extern volatile uint16_t metronomo_bpm;

/* ──────────────────────────────────────────────────────────────
 * PARÁMETROS GLOBALES DEL SINTETIZADOR
 * ────────────────────────────────────────────────────────────── */
/* 
 * 0 = Salamander Piano
 * 1 = Guitarra
 * (ondas básicas comentadas en Instrumentos.h)
 */
volatile uint8_t instrumento_actual = 0;  

/* Índice absoluto de la tecla física 0 (Ej: 48 equivale a un Do3 estándar) */
volatile uint8_t indice_escala_base = 48; 
volatile uint8_t offset_zona_ram = 0;

static const char *AUDIO_TAG = "SINTETIZADOR_RAM";

/* 1 = registra por consola el tono de la prueba 1 (diagnóstico). Poner en 0 al terminar. */
#define DIAG_TONO_PRUEBA 0
static int16_t i2s_tx_buf[DMA_BUF_FRAMES * 2];

/* ──────────────────────────────────────────────────────────────
 * ESPACIO DE TRABAJO EN RAM (CACHÉ DE INSTRUMENTO)
 * ────────────────────────────────────────────────────────────── */
#define MAX_ZONAS_RAM 11

typedef struct {
    int16_t  wavetable[1024];
    uint16_t envolvente[512];
    uint16_t env_len;
    float    duracion_s;     /* tiempo en que se recorre la envolvente completa */
} ZonaRAM_t;

/* Almacenamiento estático en RAM (.bss) para evitar desbordar el Stack */
static ZonaRAM_t instrumento_ram[MAX_ZONAS_RAM];
static uint8_t zonas_cargadas_en_ram = 0;
static uint8_t mapa_zona_tecla[32];   /* zona de RAM que usa cada tecla (según la nota inicial) */
static bool    instr_sintetico = false; /* el instrumento cargado es una onda básica */

/* ──────────────────────────────────────────────────────────────
 * PARÁMETROS DE MEZCLA Y RELEASE
 * ────────────────────────────────────────────────────────────── */
/* Ganancia FIJA por voz: el nivel de una nota ya no cambia cuando
 * entran o salen otras (antes se dividía por la cantidad de voces). */
#define MEZCLA_GANANCIA       0.40f
/* Por encima de este nivel actúa el limitador suave (evita el recorte duro) */
#define MEZCLA_UMBRAL_LIM     24000.0f
/* Release al soltar la tecla (sustain OFF): caída a -60 dB en este tiempo */
#define RELEASE_TIEMPO_S      0.15f
#define RELEASE_PISO          0.001f   /* -60 dB: la voz se apaga */

/* Límite de polifonía (MAX_VOCES en IO_config.h). Si llega una nota y ya
 * suenan MAX_VOCES voces, se roba una: primero la más vieja entre las teclas
 * que ya no están apretadas; si todas lo están, la más vieja de todas.
 * La voz robada no se corta en seco (haría clic): se apaga con un fade
 * rápido desde su nivel actual. */
#if !defined(MAX_VOCES) || (MAX_VOCES < 1) || (MAX_VOCES > 32)
#error "MAX_VOCES (IO_config.h) debe estar entre 1 y 32"
#endif
#ifndef ROBO_TIEMPO_S
#define ROBO_TIEMPO_S         0.010f   /* fade de la voz robada: -60 dB en 10 ms */
#endif

static float coef_release = 0.0f;      /* factor por muestra, se calcula al iniciar */
static float coef_robo    = 0.0f;      /* ídem para el fade de una voz robada        */

/* Bandera de seguridad para no reproducir audio mientras se copia memoria */
static volatile bool motor_audio_activo = false;

/* ──────────────────────────────────────────────────────────────
 * FUNCIÓN DE CARGA A RAM (Caché Dinámico)
 * ────────────────────────────────────────────────────────────── */
static void cargar_instrumento_en_ram(uint8_t id_instrumento, uint8_t nota_inicio)
{
    motor_audio_activo = false; /* Suspender DSP */

    if (id_instrumento >= INSTR_CANTIDAD) id_instrumento = 0;
    const InstrumentoInfo_t *info = &instrumentos_tabla[id_instrumento];

    /* Qué zonas del banco (flash) se copian y qué zona usa cada tecla */
    PlanZonas_t plan;
    instr_planificar(id_instrumento, nota_inicio, &plan);

    /* Copia masiva y ultra-rápida de bloques de memoria */
    for (int i = 0; i < plan.cantidad; i++) {
        const ZonaWavetable_t *z = &info->banco[plan.banco_idx[i]];
        memcpy(instrumento_ram[i].wavetable,  z->wavetable,  sizeof(int16_t)  * 1024);
        memcpy(instrumento_ram[i].envolvente, z->envolvente, sizeof(uint16_t) * info->env_len);
        instrumento_ram[i].env_len    = info->env_len;
        instrumento_ram[i].duracion_s = instr_duracion_zona(info, z);
    }
    memcpy(mapa_zona_tecla, plan.zona_de_tecla, sizeof(mapa_zona_tecla));
    instr_sintetico = info->sintetico;

    zonas_cargadas_en_ram = plan.cantidad;
    instrumento_actual = id_instrumento;

    motor_audio_activo = true; /* Reactivar DSP */
    ESP_LOGI(AUDIO_TAG, "%s a RAM: nota inicial %d, %d zonas",
             info->nombre, nota_inicio, zonas_cargadas_en_ram);
}

/* ──────────────────────────────────────────────────────────────
 * ESTRUCTURA DE VOZ POLIFÓNICA
 * ────────────────────────────────────────────────────────────── */
typedef struct {
    bool     activa;        
    float    fase_onda;     
    float    paso_onda;     
    float    fase_env;      
    float    paso_env;      
    uint8_t  zona_asignada;
    bool     liberando;     /* en release: tecla soltada con sustain OFF, o voz robada */
    bool     robada;        /* se apaga para dejar lugar a otra nota: ya no cuenta en MAX_VOCES */
    float    ganancia_rel;  /* ganancia del release: 1.0 -> 0 */
    float    coef_rel;      /* factor por muestra del release: coef_release o coef_robo */
    uint32_t orden;         /* número de nota-on: menor = más vieja */
} VozDDS_t;

static VozDDS_t voces[32];
static uint32_t voces_orden = 0;   /* contador de nota-on (antigüedad de cada voz) */

/* ──────────────────────────────────────────────────────────────
 * LÍMITE DE POLIFONÍA: hace lugar para la nota de la tecla 'tecla'
 *   teclas_apretadas: estado actual de las 32 teclas (bit en 1 = apretada)
 *   orden_bloque:     voces_orden al empezar este bloque. Una voz con
 *                     orden >= orden_bloque se encendió en este mismo bloque y
 *                     todavía no sonó: se apaga sin fade (no hay clic posible).
 * Se llama ANTES de inicializar la voz de 'tecla'.
 * ────────────────────────────────────────────────────────────── */
static void voces_hacer_lugar(int tecla, uint32_t teclas_apretadas, uint32_t orden_bloque)
{
    /* Re-pulsación de una tecla que todavía suena: reutiliza su propia voz */
    if (voces[tecla].activa && !voces[tecla].robada) return;

    int sonando = 0;
    int vieja_suelta   = -1;   /* la más vieja entre las teclas ya soltadas */
    int vieja_apretada = -1;   /* la más vieja entre las teclas apretadas   */

    for (int v = 0; v < 32; v++) {
        if (!voces[v].activa || voces[v].robada) continue;
        sonando++;
        if ((teclas_apretadas >> v) & 1U) {
            if (vieja_apretada < 0 || voces[v].orden < voces[vieja_apretada].orden) vieja_apretada = v;
        } else {
            if (vieja_suelta < 0 || voces[v].orden < voces[vieja_suelta].orden) vieja_suelta = v;
        }
    }
    if (sonando < MAX_VOCES) return;

    int v = (vieja_suelta >= 0) ? vieja_suelta : vieja_apretada;
    if (v < 0) return;

    if (voces[v].orden >= orden_bloque) {
        voces[v].activa = false;            /* todavía no sonó: se apaga sin clic */
    } else {
        voces[v].robada    = true;          /* fade rápido desde su nivel actual */
        voces[v].liberando = true;
        voces[v].coef_rel  = coef_robo;
    }
}

// variables para el metronomo y motor de click
#define METRONOMO_BPM_DEFAULT      40
#define METRO_TWO_PI               6.283185307179586f

static const uint8_t metronomo_pulsos_por_compas[] = {
    0, // METRONOMO_OFF (no se usa)
    3, // METRONOMO_3_4
    4, // METRONOMO_4_4
    5, // METRONOMO_5_4
    6, // METRONOMO_6_8
    7  // METRONOMO_7_8
};

typedef struct {
    bool     activo;
    uint16_t muestras_restantes;
    uint16_t muestras_totales;
    float    fase;
    float    paso;
    int16_t  amplitud_inicial;
} ClickMetronomo_t;

static ClickMetronomo_t click_metro = {0};
static uint32_t muestra_contador_metronomo = 0;
static uint8_t  pulso_actual_metronomo = 0;

static void metronomo_disparar_click(bool acento)
{
    click_metro.activo = true;
    click_metro.paso = (acento ? 1800.0f : 1000.0f) / (float)SAMPLE_RATE_HZ;
    click_metro.fase = 0.0f;
    click_metro.muestras_totales = acento
        ? (uint16_t)(SAMPLE_RATE_HZ * 0.015f)
        : (uint16_t)(SAMPLE_RATE_HZ * 0.010f);
    click_metro.muestras_restantes = click_metro.muestras_totales;
    click_metro.amplitud_inicial = acento ? 9000 : 6000;
}

static int32_t metronomo_generar_muestra(void)
{
    if (!click_metro.activo) return 0;
    float envolvente = (float)click_metro.muestras_restantes / (float)click_metro.muestras_totales;
    int32_t muestra = (int32_t)(sinf(click_metro.fase * METRO_TWO_PI)
                                 * click_metro.amplitud_inicial * envolvente);
    click_metro.fase += click_metro.paso;
    if (click_metro.fase >= 1.0f) click_metro.fase -= 1.0f;
    if (--click_metro.muestras_restantes == 0) click_metro.activo = false;
    return muestra;
}

/* ──────────────────────────────────────────────────────────────
 * TAREA PRINCIPAL DE AUDIO
 * ────────────────────────────────────────────────────────────── */
static void audio_playback_task(void *arg)
{
    i2s_chan_handle_t tx_chan = (i2s_chan_handle_t)arg;
    size_t bytes_written;
    extern QueueHandle_t cola_de_datos_del_teclado;

    uint32_t tabla_estado_local = 0;
    uint32_t estado_anterior    = 0;

    for(int i=0; i<32; i++) { voces[i].activa = false; }

    /* Release exponencial: ganancia *= coef cada muestra, -60 dB en RELEASE_TIEMPO_S */
    coef_release = expf(logf(RELEASE_PISO) / ((float)SAMPLE_RATE_HZ * RELEASE_TIEMPO_S));
    /* Mismo mecanismo para la voz robada, pero -60 dB en ROBO_TIEMPO_S */
    coef_robo    = expf(logf(RELEASE_PISO) / ((float)SAMPLE_RATE_HZ * ROBO_TIEMPO_S));

    /* Variable local para detectar cambios hechos por la OLED */
    uint8_t instrumento_cargado_actualmente = 255;
    uint8_t nota_inicio_cargada = 255;

    /* --- VARIABLES EXCLUSIVAS PARA MODO DE PRUEBA --- */
    float fase_test_audio = 0.0f;   /* [rad] tono de prueba de 440 Hz */
    const float paso_test_audio = 2.0f * (float)M_PI * 440.0f / (float)SAMPLE_RATE_HZ;

    while (1) {
        prueba_estres_inicio_bloque();   /* PRUEBA ESTRES: empieza el cálculo del bloque */
        int pe_voces = 0;                /* PRUEBA ESTRES: voces calculadas en el bloque */
        
        /* Vigilar si la OLED cambió el instrumento seleccionado o la nota inicial */
        if (instrumento_actual != instrumento_cargado_actualmente ||
            indice_escala_base != nota_inicio_cargada) {
            uint8_t inst_pedido = instrumento_actual;
            uint8_t base_pedida = indice_escala_base;
            cargar_instrumento_en_ram(inst_pedido, base_pedida);
            instrumento_cargado_actualmente = inst_pedido;
            nota_inicio_cargada = base_pedida;
        }

        /* Drenar la cola del teclado */
        uint32_t valor_temporal;
        while(xQueueReceive(cola_de_datos_del_teclado, &valor_temporal, 0) == pdTRUE) {
            tabla_estado_local = valor_temporal;
        }

        /* ===================================================================
         * MODO NORMAL DE SÍNTESIS / APRENDIZAJE 
         * =================================================================== */
        if (inicio_activo) {
            inicio_audio_llenar_buffer(i2s_tx_buf, DMA_BUF_FRAMES);
        }
        else if (modo_funcionamiento != 2) {
            
            uint32_t flancos_subida = tabla_estado_local & ~estado_anterior;
            uint32_t flancos_bajada = ~tabla_estado_local & estado_anterior;
            estado_anterior = tabla_estado_local;

            /* Procesar control de hardware y asignación de voces */
            const uint32_t orden_bloque = voces_orden;   /* voces encendidas desde acá aún no sonaron */
            for (int k = 0; k < 32; k++) {
                if ((flancos_subida >> k) & 1U) {
                    /* Límite de polifonía: si ya suenan MAX_VOCES, se roba una */
                    voces_hacer_lugar(k, tabla_estado_local, orden_bloque);

                    float numero_nota_real = (float)(indice_escala_base + k);
                    float frec_objetivo = 440.0f * powf(2.0f, (numero_nota_real - 69.0f) / 12.0f);

                    uint8_t idx_zona = mapa_zona_tecla[k];
                    if (idx_zona >= zonas_cargadas_en_ram) {
                        idx_zona = zonas_cargadas_en_ram - 1; 
                    }

                    /* La envolvente completa (puntos 0..env_len-1) se recorre en la
                       duración real de la muestra, igual que en la emulación de Octave */
                    voces[k].zona_asignada = idx_zona;
                    voces[k].paso_onda = (frec_objetivo * 1024.0f) / (float)SAMPLE_RATE_HZ;
                    voces[k].paso_env  = (float)(instrumento_ram[idx_zona].env_len - 1)
                                       / ((float)SAMPLE_RATE_HZ * instrumento_ram[idx_zona].duracion_s);
                    voces[k].fase_onda    = 0.0f;
                    voces[k].fase_env     = 0.0f;
                    voces[k].liberando    = false;
                    voces[k].robada       = false;
                    voces[k].ganancia_rel = 1.0f;
                    voces[k].coef_rel     = coef_release;
                    voces[k].orden        = voces_orden++;
                    voces[k].activa       = true;
                }
                
                if ((flancos_bajada >> k) & 1U) {
                    if (!sustain_activado && voces[k].activa && !voces[k].robada) {
                        /* Release real: atenúa desde el nivel actual, sin acelerar la envolvente.
                           Una voz robada ya está en su fade rápido y lo conserva. */
                        voces[k].liberando = true;
                    }
                }
            }
#if PRUEBA_ESTRES
            for (int k = 0; k < 32; k++) pe_voces += voces[k].activa ? 1 : 0;   /* PRUEBA ESTRES */
#endif
            uint32_t muestras_por_pulso_actual = (SAMPLE_RATE_HZ * 60UL) / metronomo_bpm;
            /* Renderizado del buffer DMA (Solo si el motor no está en pausa) */
            for (int i = 0; i < DMA_BUF_FRAMES; i++) {
                int32_t mezcla_acumulador = 0;

                if (motor_audio_activo) {
                    for (int k = 0; k < 32; k++) {
                        if (voces[k].activa) {
                            uint8_t z = voces[k].zona_asignada;
                            
                            // =======================================================
                            // 1. LECTURA DE WAVETABLE CON INTERPOLACIÓN LINEAL
                            // =======================================================
                            float f_idx = voces[k].fase_onda;
                            uint16_t idx1 = (uint16_t)f_idx;
                            uint16_t idx2 = (idx1 + 1) & 1023; // Tamaño de la onda es 1024
                            float frac = f_idx - (float)idx1;
                            
                            int32_t s1 = instrumento_ram[z].wavetable[idx1];
                            int32_t s2 = instrumento_ram[z].wavetable[idx2];
                            
                            int32_t muestra_cruda = s1 + (int32_t)((s2 - s1) * frac);
                            
                            // =======================================================
                            // 2. LECTURA DE LA ENVOLVENTE (Con Bypass para Sintéticos)
                            // =======================================================
                            int32_t valor_env;

                            if (instr_sintetico) {
                                // Ondas Básicas: Envolvente matemática plana "Gate"
                                float progreso = voces[k].fase_env / (float)instrumento_ram[z].env_len;
                                
                                if (progreso < 0.02f) {
                                    valor_env = (int32_t)((progreso / 0.02f) * 65535.0f); 
                                } else if (progreso > 0.98f) {
                                    valor_env = (int32_t)(((1.0f - progreso) / 0.02f) * 65535.0f); 
                                } else {
                                    valor_env = 65535; 
                                }
                            } else {
                                // Acústicos (Salamander Piano y Guitarra): interpolación lineal
                                float f_idx_env = voces[k].fase_env;
                                uint16_t idx_env1 = (uint16_t)f_idx_env;
                                uint16_t idx_env2 = idx_env1 + 1;
                                
                                if (idx_env2 >= instrumento_ram[z].env_len) {
                                    idx_env2 = instrumento_ram[z].env_len - 1;
                                    if (idx_env1 >= instrumento_ram[z].env_len) idx_env1 = idx_env2;
                                }
                                
                                float frac_env = f_idx_env - (float)idx_env1;
                                int32_t e1 = instrumento_ram[z].envolvente[idx_env1];
                                int32_t e2 = instrumento_ram[z].envolvente[idx_env2];
                                
                                valor_env = e1 + (int32_t)((e2 - e1) * frac_env);
                            }
                            
                            // =======================================================
                            // 3. MEZCLA Y AVANCE DE FASE
                            // =======================================================
                            int32_t muestra_voz = (muestra_cruda * valor_env) >> 16;
                            if (voces[k].liberando) {
                                muestra_voz = (int32_t)((float)muestra_voz * voces[k].ganancia_rel);
                                voces[k].ganancia_rel *= voces[k].coef_rel;   /* release normal o fade de robo */
                                if (voces[k].ganancia_rel < RELEASE_PISO) voces[k].activa = false;
                            }
                            mezcla_acumulador += muestra_voz;
                            
                            // Avanzar onda
                            voces[k].fase_onda += voces[k].paso_onda;
                            if (voces[k].fase_onda >= 1024.0f) {
                                voces[k].fase_onda -= 1024.0f;
                            }
                            
                            // Avanzar envolvente
                            voces[k].fase_env += voces[k].paso_env;
                            if (voces[k].fase_env >= (float)(instrumento_ram[z].env_len - 1)) {
                                voces[k].activa = false;
                            }
                        }
                    }
                }

                // =======================================================
                // MEZCLA CON GANANCIA FIJA + LIMITADOR SUAVE
                // =======================================================
                float mezcla_f = (float)mezcla_acumulador * MEZCLA_GANANCIA;
                float mag = fabsf(mezcla_f);
                if (mag > MEZCLA_UMBRAL_LIM) {
                    /* Compresión progresiva: tiende a 32767 sin recortar en seco */
                    const float resto = 32767.0f - MEZCLA_UMBRAL_LIM;
                    float exceso = mag - MEZCLA_UMBRAL_LIM;
                    mag = MEZCLA_UMBRAL_LIM + exceso / (1.0f + exceso / resto);
                    mezcla_f = (mezcla_f < 0.0f) ? -mag : mag;
                }
                int32_t sample_out = (int32_t)mezcla_f;

                // =======================================================
                // FILTRO PASA-BAJOS (LPF) SOLO PARA ONDAS FUNDAMENTALES
                // =======================================================
                if (instr_sintetico) {
                    static float lpf_estado = 0.0f;
                    float alpha = 0.3f;
                    lpf_estado = (sample_out * alpha) + (lpf_estado * (1.0f - alpha));
                    sample_out = (int32_t)lpf_estado;
                }

                if (metronomo_compas != METRONOMO_OFF) {
                    if (muestra_contador_metronomo == 0) {
                        metronomo_disparar_click(pulso_actual_metronomo == 0);
                    }
                    sample_out += metronomo_generar_muestra();
                    if (++muestra_contador_metronomo >= muestras_por_pulso_actual) {
                        muestra_contador_metronomo = 0;
                        pulso_actual_metronomo = (pulso_actual_metronomo + 1)
                                                % metronomo_pulsos_por_compas[metronomo_compas];
                    }
                }

                if (sample_out >  32767) sample_out =  32767;
                if (sample_out < -32767) sample_out = -32767;
                i2s_tx_buf[i * 2]     = (int16_t)sample_out;
                i2s_tx_buf[i * 2 + 1] = (int16_t)sample_out;
            }
        } 
        /* ===================================================================
         * MODO PRUEBA DE HARDWARE (Diagnóstico)
         * =================================================================== */
        else if (subestado_prueba == PRUEBA_SD) {
            if (sd_test_stop_request) {
                sd_test_detener(tx_chan);
                sd_test_stop_request = false;
            }

            if (sd_test_play_request) {
                sd_test_play_request = false;
                sd_test_iniciar(tx_chan);
            }

            if (sd_test_reproduciendo) {
                sd_test_llenar_buffer(i2s_tx_buf, DMA_BUF_FRAMES);

                if (!sd_test_reproduciendo) {
                    sd_reconfigurar_i2s(tx_chan, SAMPLE_RATE_HZ);
                }
            } else {
                memset(i2s_tx_buf, 0, sizeof(i2s_tx_buf));
            }
        }
        else {
            for (int i = 0; i < DMA_BUF_FRAMES; i++) {
                int32_t sample_out = 0;

                if (subestado_prueba == PRUEBA_AUDIO) {
                    /* Prueba 2: tono de 440 Hz a media escala, con la tira LED apagada */
                    sample_out = (int32_t)(sinf(fase_test_audio) * 16383.0f);

                    fase_test_audio += paso_test_audio;
                    if (fase_test_audio >= 2.0f * (float)M_PI) {
                        fase_test_audio -= 2.0f * (float)M_PI;
                    }
                }

                i2s_tx_buf[i * 2]     = (int16_t)sample_out;
                i2s_tx_buf[i * 2 + 1] = (int16_t)sample_out;
            }
        }

        prueba_estres_fin_bloque(DMA_BUF_FRAMES, pe_voces);   /* PRUEBA ESTRES: fin del cálculo, antes de esperar al I2S */

        /* Transmisión al DAC */
        ESP_ERROR_CHECK(i2s_channel_write(
            tx_chan,
            i2s_tx_buf,
            sizeof(i2s_tx_buf),
            &bytes_written,
            portMAX_DELAY
        ));

#if DIAG_TONO_PRUEBA
        /* DIAGNÓSTICO (prueba 2, audio): una línea por segundo con lo que
           realmente se entregó al I2S. Si el pico sigue en ~16383 y los bytes
           en 4096 mientras no se oye nada, el ESP32 está enviando el tono y
           el corte ocurre después del pin DIN (en el PCM5102A o su conexión). */
        if (modo_funcionamiento == 2 && subestado_prueba == PRUEBA_AUDIO) {
            static uint32_t diag_buffers = 0;
            static int32_t  diag_pico    = 0;
            for (int i = 0; i < DMA_BUF_FRAMES * 2; i += 2) {
                int32_t v = i2s_tx_buf[i] < 0 ? -i2s_tx_buf[i] : i2s_tx_buf[i];
                if (v > diag_pico) diag_pico = v;
            }
            if (++diag_buffers % 86 == 0) {          /* 86 buffers de 512 = ~1 s */
                ESP_LOGI(AUDIO_TAG, "DIAG tono: t=%lus  bytes=%u  pico=%ld",
                         (unsigned long)(diag_buffers / 86),
                         (unsigned)bytes_written, (long)diag_pico);
                diag_pico = 0;
            }
        }
#endif
    }
}

#endif /* TASK_AUDIO_H */