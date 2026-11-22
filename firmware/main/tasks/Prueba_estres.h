/* =============================================================================
 *  Prueba_estres.h  —  Prueba temporal de carga de cálculo del sintetizador
 *  Autores: Luis Carlos Hubner y Matias Emanuel Richart
 *
 *  Dispara teclas virtuales en grupos de 1, 4, 8 y 16 (teclas 0 a N-1),
 *  las mantiene presionadas y mide, bloque por bloque, cuánto tarda
 *  audio_playback_task en calcular cada buffer DMA. Al cerrar cada ventana
 *  imprime por el monitor serie el tiempo promedio y el máximo, la carga de
 *  CPU respecto del tiempo real del bloque (DMA_BUF_FRAMES / SAMPLE_RATE_HZ) y
 *  las voces activas. Al final imprime una tabla resumen, un ajuste lineal
 *  tiempo-voces y las filas listas para pegar en una tabla de LaTeX.
 *
 *  Con el límite de polifonía (MAX_VOCES en IO_config.h) un caso con más
 *  teclas que MAX_VOCES hace sonar solo MAX_VOCES voces: el caso de 16 teclas
 *  sirve también para comprobar que el límite funciona.
 *
 *  Las teclas virtuales se suman a la lectura del GPIO de cada tecla dentro de
 *  escanear_teclado(): pasan por el antirrebote y generan los mismos eventos
 *  que una tecla real (cola del audio, tira LED, OLED y aprendizaje).
 *
 *  El tiempo medido es tiempo real transcurrido en el núcleo 1: incluye las
 *  interrupciones que atienda ese núcleo, que es lo que decide si el buffer
 *  llega a tiempo al I2S.
 *
 *  Integración (marcada con "PRUEBA ESTRES" en cada archivo):
 *    main.c          crea tarea_prueba_estres (núcleo 0, prioridad 1)
 *    Task_audio.h    prueba_estres_inicio_bloque() al comenzar cada vuelta del
 *                    bucle, cuenta de voces activas y prueba_estres_fin_bloque()
 *                    justo antes de i2s_channel_write()
 *    Task_Teclado.h  estado_actual |= prueba_estres_tecla(tecla)
 *
 *  Uso: poner PRUEBA_ESTRES en 1, grabar, dejar el equipo en Modo Sintetizador
 *  con el instrumento elegido y no tocar el teclado hasta que aparezca
 *  "Prueba terminada". Para volver al firmware normal: PRUEBA_ESTRES en 0
 *  (las líneas agregadas pueden quedar, no hacen nada).
 * =============================================================================
 */

#ifndef PRUEBA_ESTRES_H
#define PRUEBA_ESTRES_H

#ifndef PRUEBA_ESTRES
#define PRUEBA_ESTRES   0          /* 1 = prueba activa | 0 = firmware normal */
#endif

#include <stdint.h>
#include <stdbool.h>

#if PRUEBA_ESTRES

/* Usa variables de Task_OLED.h (modo, sustain, metrónomo): respetar el orden de main.c */
#ifndef TASK_OLED_H
#error "Prueba_estres.h debe incluirse despues de Task_OLED.h (orden de main.c)"
#endif

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_cpu.h"
#include "IO_config.h"       /* SAMPLE_RATE_HZ, MAX_VOCES          */
#include "Instrumentos.h"    /* instrumentos_tabla, INSTR_CANTIDAD */

/* ──────────────────────────────────────────────────────────────
 *  PARÁMETROS
 * ────────────────────────────────────────────────────────────── */
#ifndef PE_CASOS
#define PE_CASOS              { 1, 4, 8, 16 }       /* teclas por caso: de 0 a N-1 */
#endif
#ifndef PE_VENTANA_MS
#define PE_VENTANA_MS         1000    /* análisis de cada caso                          */
#endif
#ifndef PE_ESTABILIZACION_MS
#define PE_ESTABILIZACION_MS  100     /* tras el disparo: el antirrebote pide 3 lecturas */
#endif
#ifndef PE_PAUSA_MS
#define PE_PAUSA_MS           1000    /* pausa mínima entre casos                       */
#endif
#ifndef PE_SILENCIO_MAX_MS
#define PE_SILENCIO_MAX_MS    6000    /* espera máx. a que se apaguen las voces (sustain) */
#endif
#ifndef PE_ESPERA_INICIAL_MS
#define PE_ESPERA_INICIAL_MS  15000   /* tiempo para dejar el equipo listo al encender  */
#endif
#ifndef PE_CORRIDAS
#define PE_CORRIDAS           1       /* repeticiones de la secuencia (2: piano y guitarra) */
#endif
#ifndef PE_PAUSA_CORRIDAS_MS
#define PE_PAUSA_CORRIDAS_MS  15000   /* entre corridas, para cambiar de instrumento    */
#endif
#ifndef PE_DECIMAL_LATEX
#define PE_DECIMAL_LATEX      ','     /* separador decimal de las filas para LaTeX      */
#endif

#define PE_FS_HZ              SAMPLE_RATE_HZ

#if defined(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
#define PE_CPU_MHZ            CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
#else
#define PE_CPU_MHZ            240
#endif

/* ──────────────────────────────────────────────────────────────
 *  ESTADO COMPARTIDO
 * ────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t bloques;          /* bloques medidos en la ventana              */
    uint64_t suma_us;          /* suma de los tiempos de cálculo             */
    uint32_t max_us;
    uint32_t muestras;         /* frames por bloque informados               */
    uint64_t suma_periodo_us;  /* suma de los periodos entre bloques         */
    uint32_t n_periodos;
    uint32_t excedidos;        /* bloques que tardaron más que su duración   */
    int      voces_max;
    int      voces_fin;
    int      nucleo;
} pe_estad_t;

typedef struct {
    int      teclas;
    int      voces;            /* máximo observado en la ventana             */
    uint32_t bloques;
    uint32_t muestras;
    uint32_t max_us;
    uint32_t excedidos;
    double   prom_us;
    double   periodo_us;       /* periodo real medido del bucle de audio     */
    double   presupuesto_us;   /* duración del bloque: frames / Fs           */
} pe_res_t;

static portMUX_TYPE      pe_mux = portMUX_INITIALIZER_UNLOCKED;
static pe_estad_t        pe_est;
static volatile uint32_t pe_mascara = 0;        /* teclas virtuales presionadas */
static volatile bool     pe_midiendo = false;
static volatile int      pe_voces_ultimo = -1;  /* voces del último bloque      */
static int64_t           pe_t0 = 0;
static int64_t           pe_t_prev = 0;
static uint32_t          pe_periodo_us = 0;

/* ──────────────────────────────────────────────────────────────
 *  LLAMADAS DESDE audio_playback_task
 * ────────────────────────────────────────────────────────────── */
static inline void prueba_estres_inicio_bloque(void)
{
    int64_t t = esp_timer_get_time();
    pe_periodo_us = pe_t_prev ? (uint32_t)(t - pe_t_prev) : 0;
    pe_t_prev = t;
    pe_t0 = t;
}

static inline void prueba_estres_fin_bloque(uint32_t muestras, int voces_activas)
{
    uint32_t dt = (uint32_t)(esp_timer_get_time() - pe_t0);

    pe_voces_ultimo = voces_activas;
    if (!pe_midiendo) return;

    portENTER_CRITICAL(&pe_mux);
    if (pe_midiendo) {
        pe_est.bloques++;
        pe_est.suma_us += dt;
        if (dt > pe_est.max_us) pe_est.max_us = dt;
        pe_est.muestras = muestras;
        if (pe_periodo_us) {
            pe_est.suma_periodo_us += pe_periodo_us;
            pe_est.n_periodos++;
        }
        if ((uint64_t)dt * PE_FS_HZ > (uint64_t)muestras * 1000000ULL) pe_est.excedidos++;
        if (voces_activas > pe_est.voces_max) pe_est.voces_max = voces_activas;
        pe_est.voces_fin = voces_activas;
        pe_est.nucleo = esp_cpu_get_core_id();
    }
    portEXIT_CRITICAL(&pe_mux);
}

/* ──────────────────────────────────────────────────────────────
 *  LLAMADA DESDE escanear_teclado(): 1 si la tecla está presionada en la prueba
 * ────────────────────────────────────────────────────────────── */
static inline bool prueba_estres_tecla(int tecla)
{
    return (tecla >= 0) && (tecla < 32) && ((pe_mascara >> tecla) & 1u);
}

/* ──────────────────────────────────────────────────────────────
 *  INTERNO
 * ────────────────────────────────────────────────────────────── */
static void pe_reiniciar(void)
{
    portENTER_CRITICAL(&pe_mux);
    memset(&pe_est, 0, sizeof(pe_est));
    pe_est.voces_max = -1;
    pe_est.voces_fin = -1;
    pe_est.nucleo = -1;
    portEXIT_CRITICAL(&pe_mux);
}

/* Número con un decimal y el separador elegido, para las filas de LaTeX */
static const char *pe_num(char *buf, size_t n, double x)
{
    if (x < 0.0) x = 0.0;
    if (x > 999999.0) x = 999999.0;
    unsigned long decimas = (unsigned long)(x * 10.0 + 0.5);
    snprintf(buf, n, "%lu%c%lu", decimas / 10, PE_DECIMAL_LATEX, decimas % 10);
    return buf;
}

/* Instrumento y opciones vigentes: cambian el resultado, quedan en el registro */
static void pe_imprimir_contexto(void)
{
    uint8_t id = (instrumento_actual < INSTR_CANTIDAD) ? instrumento_actual : 0;
    char nota[8];
    instr_nombre_nota(indice_escala_base, nota, sizeof(nota));

    printf("[PE] Instrumento: %s | nota inicial: %s (%d) | sustain: %s | metronomo: %s\n",
           instrumentos_tabla[id].nombre, nota, (int)indice_escala_base,
           sustain_activado ? "ON" : "OFF",
           (metronomo_compas != METRONOMO_OFF) ? "ON" : "OFF");
    if (modo_funcionamiento != 1) {
        printf("[PE] ATENCION: el equipo no esta en Modo Sintetizador (modo %d)\n",
               (int)modo_funcionamiento);
    }
}

/* Un caso: dispara n teclas, mide durante la ventana e imprime */
static void pe_caso(int n, int indice, int total, pe_res_t *r)
{
    if (n < 1) n = 1;
    if (n > 32) n = 32;
    uint32_t mascara = (n == 32) ? 0xFFFFFFFFu : ((1u << n) - 1u);

    pe_reiniciar();
    pe_mascara = mascara;                         /* disparo: teclas 0 a n-1 */
    if (PE_ESTABILIZACION_MS > 0) {
        vTaskDelay(pdMS_TO_TICKS(PE_ESTABILIZACION_MS));
        pe_reiniciar();
    }
    pe_midiendo = true;
    vTaskDelay(pdMS_TO_TICKS(PE_VENTANA_MS));
    pe_midiendo = false;
    pe_mascara = 0;                               /* se sueltan todas las teclas */

    pe_estad_t e;
    portENTER_CRITICAL(&pe_mux);
    e = pe_est;
    portEXIT_CRITICAL(&pe_mux);

    memset(r, 0, sizeof(*r));
    r->teclas   = n;
    r->voces    = e.voces_max;
    r->bloques  = e.bloques;
    r->muestras = e.muestras;

    printf("\n[PE] Caso %d/%d: %d tecla%s (0 a %d), ventana de %d ms\n",
           indice, total, n, (n == 1) ? "" : "s", n - 1, (int)PE_VENTANA_MS);

    if (e.bloques == 0 || e.muestras == 0) {
        printf("[PE]   No se midio ningun bloque: revisar las llamadas en audio_playback_task.\n");
        return;
    }

    r->max_us         = e.max_us;
    r->excedidos      = e.excedidos;
    r->prom_us        = (double)e.suma_us / e.bloques;
    r->periodo_us     = e.n_periodos ? (double)e.suma_periodo_us / e.n_periodos : 0.0;
    r->presupuesto_us = 1e6 * e.muestras / PE_FS_HZ;

    double carga_prom = 100.0 * r->prom_us / r->presupuesto_us;
    double carga_pico = 100.0 * r->max_us / r->presupuesto_us;
    double ciclos     = r->prom_us * PE_CPU_MHZ / e.muestras;

    printf("[PE]   Bloques medidos: %lu de %lu muestras, audio en el nucleo %d\n",
           (unsigned long)e.bloques, (unsigned long)e.muestras, e.nucleo);
    printf("[PE]   Voces activas: maximo %d, al final de la ventana %d\n",
           e.voces_max, e.voces_fin);
    /* Con el límite de polifonía no pueden sonar más de MAX_VOCES voces */
    const int esperadas = (n < MAX_VOCES) ? n : MAX_VOCES;
    if (n > MAX_VOCES) {
        printf("[PE]   Limite de polifonia: %d teclas, deben sonar %d voces (MAX_VOCES)\n",
               n, MAX_VOCES);
    }
    if (e.voces_max < esperadas) {
        printf("[PE]   ATENCION: %d teclas disparadas pero %d voces: el sintetizador no\n"
               "[PE]   recibio todas las teclas o no esta en Modo Sintetizador.\n",
               n, e.voces_max);
    } else if (e.voces_max > esperadas) {
        printf("[PE]   ATENCION: %d voces con MAX_VOCES = %d: el limite de polifonia no actua.\n",
               e.voces_max, MAX_VOCES);
    }
    printf("[PE]   Tiempo de calculo por bloque: promedio %.1f us | maximo %lu us\n",
           r->prom_us, (unsigned long)e.max_us);
    printf("[PE]   Tiempo disponible por bloque: %.1f us (periodo medido del bucle: %.1f us)\n",
           r->presupuesto_us, r->periodo_us);
    printf("[PE]   Carga de CPU: promedio %.1f %% | pico %.1f %% | %.0f ciclos por muestra\n",
           carga_prom, carga_pico, ciclos);
    printf("[PE]   Bloques que tardaron mas que su duracion: %lu\n", (unsigned long)e.excedidos);

    /* Si el bucle no se atrasa, su periodo real debe coincidir con frames / Fs */
    double desvio = (r->periodo_us - r->presupuesto_us) / r->presupuesto_us;
    if (e.excedidos == 0 && r->periodo_us > 0.0 && (desvio > 0.05 || desvio < -0.05)) {
        printf("[PE]   ATENCION: el periodo medido difiere %.0f %% del esperado (DMA_BUF_FRAMES / SAMPLE_RATE_HZ).\n",
               100.0 * desvio);
    }
}

/* Tabla resumen, ajuste lineal y filas para LaTeX */
static void pe_resumen(const pe_res_t *r, int n, int corrida)
{
    char b1[24], b2[24], b3[24];

    printf("\n[PE] ============ RESUMEN corrida %d | CPU %d MHz | Fs %d Hz ============\n",
           corrida, (int)PE_CPU_MHZ, (int)PE_FS_HZ);
    pe_imprimir_contexto();
    printf("[PE] Teclas  Voces  Prom[us]  Max[us]  Carga prom[%%]  Carga pico[%%]  Excedidos\n");
    for (int i = 0; i < n; i++) {
        if (r[i].bloques == 0 || r[i].muestras == 0) continue;
        printf("[PE] %6d  %5d  %8.1f  %7lu  %13.1f  %13.1f  %9lu\n",
               r[i].teclas, r[i].voces, r[i].prom_us, (unsigned long)r[i].max_us,
               100.0 * r[i].prom_us / r[i].presupuesto_us,
               100.0 * r[i].max_us / r[i].presupuesto_us,
               (unsigned long)r[i].excedidos);
    }

    /* Ajuste lineal t = a + b*voces por mínimos cuadrados */
    double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0, presupuesto = 0, muestras = 0;
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (r[i].bloques == 0 || r[i].muestras == 0 || r[i].voces <= 0) continue;
        double x = r[i].voces;
        double y = r[i].prom_us;
        sx += x; sy += y; sxx += x * x; sxy += x * y; syy += y * y;
        presupuesto = r[i].presupuesto_us;
        muestras = r[i].muestras;
        k++;
    }
    double den_x = k * sxx - sx * sx;
    double den_y = k * syy - sy * sy;
    if (k >= 2 && den_x > 0 && den_y > 0) {
        double num = k * sxy - sx * sy;
        double b = num / den_x;
        double a = (sy - b * sx) / k;
        double r2 = num * num / (den_x * den_y);
        printf("[PE] Ajuste lineal t = a + b*voces: a = %.1f us (costo fijo por bloque), b = %.2f us por voz\n",
               a, b);
        printf("[PE]   b equivale a %.1f ciclos por voz y por muestra; R^2 = %.4f\n",
               b * PE_CPU_MHZ / muestras, r2);
        if (b > 0) {
            printf("[PE] Polifonia maxima estimada: %d voces al 100 %% del tiempo disponible,"
                   " %d voces dejando 20 %% libre\n",
                   (int)((presupuesto - a) / b), (int)((0.8 * presupuesto - a) / b));
        }
    }

    printf("[PE] Filas para LaTeX: teclas & voces & prom [us] & max [us] & carga prom [%%] & carga pico [%%]\n");
    for (int i = 0; i < n; i++) {
        if (r[i].bloques == 0 || r[i].muestras == 0) continue;
        printf("%d & %d & %s & %lu & %s & %s \\\\\n", r[i].teclas, r[i].voces,
               pe_num(b1, sizeof(b1), r[i].prom_us), (unsigned long)r[i].max_us,
               pe_num(b2, sizeof(b2), 100.0 * r[i].prom_us / r[i].presupuesto_us),
               pe_num(b3, sizeof(b3), 100.0 * r[i].max_us / r[i].presupuesto_us));
    }
}

/* ──────────────────────────────────────────────────────────────
 *  TAREA FreeRTOS (se crea en main.c y se elimina al terminar)
 * ────────────────────────────────────────────────────────────── */
static void tarea_prueba_estres(void *arg)
{
    (void)arg;
    static const int casos[] = PE_CASOS;
    enum { N_CASOS = (int)(sizeof(casos) / sizeof(casos[0])) };
    pe_res_t res[N_CASOS];

    printf("\n[PE] PRUEBA DE ESTRES DE SINTESIS: comienza en %d s.\n",
           (int)(PE_ESPERA_INICIAL_MS / 1000));
    printf("[PE] Dejar el equipo en Modo Sintetizador con el instrumento elegido y no tocar el teclado.\n");
    vTaskDelay(pdMS_TO_TICKS(PE_ESPERA_INICIAL_MS));

    for (int c = 1; c <= PE_CORRIDAS; c++) {
        printf("\n[PE] Corrida %d\n", c);
        pe_imprimir_contexto();
        for (int i = 0; i < N_CASOS; i++) {
            pe_caso(casos[i], i + 1, N_CASOS, &res[i]);
            /* Con sustain las notas siguen hasta el fin de su envolvente: se espera el silencio */
            vTaskDelay(pdMS_TO_TICKS(PE_PAUSA_MS));
            for (int t = 0; t < PE_SILENCIO_MAX_MS && pe_voces_ultimo > 0; t += 50) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        pe_resumen(res, N_CASOS, c);
        if (c < PE_CORRIDAS) {
            printf("\n[PE] Corrida %d en %d s: se puede cambiar de instrumento.\n",
                   c + 1, (int)(PE_PAUSA_CORRIDAS_MS / 1000));
            vTaskDelay(pdMS_TO_TICKS(PE_PAUSA_CORRIDAS_MS));
        }
    }
    printf("\n[PE] Prueba terminada. Para volver al firmware normal: #define PRUEBA_ESTRES 0\n");
    vTaskDelete(NULL);
}

#else  /* PRUEBA_ESTRES == 0: firmware normal, las llamadas quedan vacías */

static inline void prueba_estres_inicio_bloque(void) {}
static inline void prueba_estres_fin_bloque(uint32_t muestras, int voces_activas)
{
    (void)muestras; (void)voces_activas;
}
static inline bool prueba_estres_tecla(int tecla) { (void)tecla; return false; }

#endif /* PRUEBA_ESTRES */

#endif /* PRUEBA_ESTRES_H */
