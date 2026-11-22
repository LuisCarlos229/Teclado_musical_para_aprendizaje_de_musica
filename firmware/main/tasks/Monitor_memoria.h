/* =============================================================================
 *  Monitor_memoria.h  —  Resumen de pila por tarea y de heap por monitor serie
 *  Autores: Luis Carlos Hubner y Matias Emanuel Richart
 *
 *  Cada MM_PERIODO_MS imprime, para cada tarea del firmware, la pila asignada,
 *  el mínimo histórico de pila libre (uxTaskGetStackHighWaterMark) y la pila
 *  utilizada, más el estado del heap. Los mínimos se acumulan desde el
 *  arranque: conviene usar todos los modos (menú, sintetizador con muchas
 *  teclas, aprendizaje desde flash y SD, modo prueba con WAV, metrónomo) y
 *  tomar el último resumen. Al final imprime las filas para la tabla de LaTeX.
 *
 *  En ESP-IDF la pila se expresa en bytes, tanto al crear la tarea como en
 *  uxTaskGetStackHighWaterMark().
 *
 *  Integración:
 *    main.c   #include "Monitor_memoria.h" y, al final de app_main():
 *               #if MONITOR_MEMORIA
 *                   xTaskCreatePinnedToCore(tarea_monitor_memoria, "monitor_memoria",
 *                                           MM_PILA_MONITOR, NULL, 1, NULL, 0);
 *               #endif
 *    Inicio.h #include "Monitor_memoria.h" y, en tarea_inicio, justo antes
 *             de vTaskDelete(NULL):  monitor_pila_al_terminar();
 *             (la tarea se elimina a los 2 s y no se la podría consultar después)
 *
 *  Para activarlo: MONITOR_MEMORIA en 1 (en 0 queda fuera del firmware).
 * =============================================================================
 */

#ifndef MONITOR_MEMORIA_H
#define MONITOR_MEMORIA_H

#ifndef MONITOR_MEMORIA
#define MONITOR_MEMORIA   0        /* 1 = monitor activo | 0 = firmware normal */
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if MONITOR_MEMORIA

#include "esp_system.h"
#include "esp_heap_caps.h"

#define MM_PERIODO_MS     30000    /* intervalo entre resúmenes          */
#define MM_PILA_MONITOR   3072     /* pila de esta tarea [bytes]         */
#define MM_MARGEN_MIN     512      /* menos de esto libre: aviso [bytes] */

#ifndef configMAX_TASK_NAME_LEN
#define configMAX_TASK_NAME_LEN 16
#endif

typedef struct {
    const char *nombre;       /* el de xTaskCreatePinnedToCore() en main.c */
    const char *nombre_tex;   /* como figura en la tabla del informe       */
    uint32_t    asignada;     /* bytes: MANTENER IGUAL QUE EN main.c       */
    uint32_t    min_libre;    /* UINT32_MAX = todavía sin dato             */
    bool        terminada;    /* la tarea ya se eliminó a sí misma         */
} mm_tarea_t;

static mm_tarea_t mm_tareas[] = {
    { "tarea_teclado",     "Task\\_Teclado",     2048,            UINT32_MAX, false },
    { "tarea_pcf",         "Task\\_Pulsadores",  2048,            UINT32_MAX, false },
    { "tarea_oled",        "Task\\_OLED",        4096,            UINT32_MAX, false },
    { "tarea_tira",        "Task\\_TiraLED",     2048,            UINT32_MAX, false },
    { "audio_task",        "Task\\_Audio",       4096,            UINT32_MAX, false },
    { "tarea_aprendizaje", "Task\\_Aprendizaje", 4096,            UINT32_MAX, false },
    { "tarea_inicio",      "Inicio",             3072,            UINT32_MAX, false },
    { "monitor_memoria",   "Monitor\\_memoria",  MM_PILA_MONITOR, UINT32_MAX, false },
};
#define MM_N_TAREAS  ((int)(sizeof(mm_tareas) / sizeof(mm_tareas[0])))

/* FreeRTOS guarda solo los primeros configMAX_TASK_NAME_LEN - 1 caracteres del
 * nombre (15 por defecto): "tarea_aprendizaje" queda como "tarea_aprendiza", y
 * xTaskGetHandle() aborta si recibe un nombre más largo. Se compara recortado. */
static inline bool mm_mismo_nombre(const char *a, const char *b)
{
    return strncmp(a, b, configMAX_TASK_NAME_LEN - 1) == 0;
}

static inline void mm_nombre_corto(const char *nombre, char *corto)
{
    size_t n = strnlen(nombre, configMAX_TASK_NAME_LEN - 1);
    memcpy(corto, nombre, n);
    corto[n] = '\0';
}

/* Llamar desde una tarea justo antes de su vTaskDelete(NULL) */
static inline void monitor_pila_al_terminar(void)
{
    const char *propia = pcTaskGetName(NULL);
    uint32_t libre = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
    for (int i = 0; i < MM_N_TAREAS; i++) {
        if (mm_mismo_nombre(mm_tareas[i].nombre, propia)) {
            mm_tareas[i].min_libre = libre;
            mm_tareas[i].terminada = true;
        }
    }
}

static void mm_actualizar(void)
{
    const char *propia = pcTaskGetName(NULL);
    char corto[configMAX_TASK_NAME_LEN];
    for (int i = 0; i < MM_N_TAREAS; i++) {
        if (mm_tareas[i].terminada) continue;
        bool es_propia = mm_mismo_nombre(mm_tareas[i].nombre, propia);
        TaskHandle_t h = NULL;
        if (!es_propia) {
            mm_nombre_corto(mm_tareas[i].nombre, corto);
            h = xTaskGetHandle(corto);
            if (h == NULL) continue;                                   /* no existe */
        }
        uint32_t libre = (uint32_t)uxTaskGetStackHighWaterMark(h);
        if (libre < mm_tareas[i].min_libre) mm_tareas[i].min_libre = libre;
    }
}

static void mm_imprimir(uint32_t segundos)
{
    printf("\n[MEM] ============ Pila de las tareas (t = %lu s) ============\n",
           (unsigned long)segundos);
    printf("[MEM] Tarea               Asignada  Min. libre    Usada    Uso\n");
    for (int i = 0; i < MM_N_TAREAS; i++) {
        const mm_tarea_t *t = &mm_tareas[i];
        if (t->min_libre == UINT32_MAX) {
            printf("[MEM] %-18s %8lu         sin dato (no se encontro la tarea)\n",
                   t->nombre, (unsigned long)t->asignada);
            continue;
        }
        uint32_t usada = (t->asignada > t->min_libre) ? t->asignada - t->min_libre : 0;
        printf("[MEM] %-18s %8lu  %10lu  %7lu  %4lu %%%s%s\n",
               t->nombre, (unsigned long)t->asignada, (unsigned long)t->min_libre,
               (unsigned long)usada, (unsigned long)(100u * usada / t->asignada),
               t->terminada ? "  (terminada)" : "",
               (t->min_libre < MM_MARGEN_MIN) ? "  <- poco margen" : "");
    }

    printf("[MEM] Heap libre: actual %lu B | minimo historico %lu B | bloque mayor %lu B\n",
           (unsigned long)esp_get_free_heap_size(),
           (unsigned long)esp_get_minimum_free_heap_size(),
           (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf("[MEM] RAM interna libre: %lu B | minimo historico %lu B\n",
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));

    printf("[MEM] Filas para LaTeX (tarea & asignada & minimo libre & utilizada):\n");
    for (int i = 0; i < MM_N_TAREAS; i++) {
        const mm_tarea_t *t = &mm_tareas[i];
        if (t->min_libre == UINT32_MAX) continue;
        uint32_t usada = (t->asignada > t->min_libre) ? t->asignada - t->min_libre : 0;
        printf("\\texttt{%s} & %lu bytes & %lu bytes & %lu bytes \\\\ \\hline\n",
               t->nombre_tex, (unsigned long)t->asignada,
               (unsigned long)t->min_libre, (unsigned long)usada);
    }
}

/* Tarea FreeRTOS: se crea en main.c y queda corriendo */
static void tarea_monitor_memoria(void *arg)
{
    (void)arg;
    uint32_t segundos = 0;
    printf("[MEM] Monitor de memoria activo: resumen cada %d s.\n", MM_PERIODO_MS / 1000);
    /* Se empieza a muestrear cuando tarea_inicio ya terminó: así nunca se consulta
       una tarea que se está eliminando (esa informa sola con monitor_pila_al_terminar) */
    vTaskDelay(pdMS_TO_TICKS(5000));
    for (;;) {
        /* Muestreo frecuente para no perder tareas que viven poco; impresión espaciada */
        for (int t = 0; t < MM_PERIODO_MS; t += 1000) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            mm_actualizar();
        }
        segundos += MM_PERIODO_MS / 1000;
        mm_imprimir(segundos);
    }
}

#else  /* MONITOR_MEMORIA == 0 */

static inline void monitor_pila_al_terminar(void) {}

#endif /* MONITOR_MEMORIA */

#endif /* MONITOR_MEMORIA_H */
