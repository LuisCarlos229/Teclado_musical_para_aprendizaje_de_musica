/* =============================================================================
 *  Instrumentos.h  —  Tabla de instrumentos + selección de NOTA INICIAL
 * =============================================================================
 */

#ifndef INSTRUMENTOS_H
#define INSTRUMENTOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ──────────────────────────────────────────────────────────────
 *  ESTRUCTURA DE ZONA (única definición del firmware)
 *  Se define ANTES de incluir los bancos: así la guarda
 *  ZONA_WAVETABLE_T_DEFINED hace que los .h generados por Octave no
 *  redefinan el struct. Un banco viejo sin duracion_s compila igual
 *  (el campo queda en 0 y se usa duracion_defecto_s del instrumento).
 * ────────────────────────────────────────────────────────────── */
#ifndef ZONA_WAVETABLE_T_DEFINED
#define ZONA_WAVETABLE_T_DEFINED
typedef struct {
    const int16_t*  wavetable;
    const uint16_t* envolvente;
    float           frecuencia_base;
    uint16_t        indice_tecla;
    float           duracion_s;      /* tiempo que abarca la envolvente (0 = usar el valor por defecto) */
} ZonaWavetable_t;
#endif

#include "SalamanderGrandPiano.h"
#include "guitarra.h"
/* #include "BasicWaves.h" */        /* Ondas básicas deshabilitadas por ahora */

/* ──────────────────────────────────────────────────────────────
 *  PARÁMETROS
 * ────────────────────────────────────────────────────────────── */
#ifndef MAX_ZONAS_RAM
#define MAX_ZONAS_RAM            11     /* zonas que caben en la caché de RAM  */
#endif

#define INSTR_CANTIDAD           2      /* Piano + Guitarra (ondas básicas comentadas) */
#define INSTR_TECLAS             32
#define INSTR_MAX_INICIOS        48     /* notas iniciales seleccionables (máx.) */
#define INSTR_MAX_ZONAS_BANCO    40

#define INSTR_SINT_NOTA_MIN      24     /* C1                                  */
#define INSTR_SINT_NOTA_MAX      96     /* C7 -> 6 octavas (72 semitonos)      */

typedef struct {
    const char            *nombre;
    const ZonaWavetable_t *banco;
    uint8_t                zonas;       /* cantidad de zonas del banco en flash */
    uint16_t               env_len;     /* puntos de envolvente por zona        */
    bool                   sintetico;   /* true = onda básica (rango fijo)      */
    float                  duracion_defecto_s; /* si una zona no trae duracion_s */
} InstrumentoInfo_t;

/* Mismo orden de IDs que instrumento_actual y que el menú de la OLED */
static const InstrumentoInfo_t instrumentos_tabla[INSTR_CANTIDAD] = {
    { "Salamander Piano", banco_SalamanderGrandPiano, SALAMANDERGRANDPIANO_CANTIDAD_ZONAS, SALAMANDERGRANDPIANO_ENV_LENGTH, false, 4.0f },
    { "Guitarra",         banco_guitarra,             GUITARRA_CANTIDAD_ZONAS,              GUITARRA_ENV_LENGTH,             false, 3.0f },
    /* Ondas básicas deshabilitadas por ahora. Para volver a usarlas: descomentar
       el #include "BasicWaves.h", estas filas y sus nombres en Task_OLED.h (m_inst),
       y subir INSTR_CANTIDAD a 6.
    { "Senoidal",         banco_Senoidal,             BASIC_WAVES_CANTIDAD_ZONAS,           256,                             true,  2.0f },
    { "Cuadrada",         banco_Cuadrada,             BASIC_WAVES_CANTIDAD_ZONAS,           256,                             true,  2.0f },
    { "Triangular",       banco_Triangular,           BASIC_WAVES_CANTIDAD_ZONAS,           256,                             true,  2.0f },
    { "Sierra",           banco_Sierra,               BASIC_WAVES_CANTIDAD_ZONAS,           256,                             true,  2.0f },
    */
};

/* Duración de una zona: la exportada por Octave o, si no la trae, la del instrumento */
static inline float instr_duracion_zona(const InstrumentoInfo_t *I, const ZonaWavetable_t *z)
{
    return (z->duracion_s > 0.05f) ? z->duracion_s : I->duracion_defecto_s;
}

/* ──────────────────────────────────────────────────────────────
 *  UTILIDADES DE NOTAS
 * ────────────────────────────────────────────────────────────── */
/* Nota MIDI original de una zona, a partir de su frecuencia_base */
static inline int instr_nota_de_zona(const ZonaWavetable_t *z)
{
    return (int)lroundf(69.0f + 12.0f * log2f(z->frecuencia_base / 440.0f));
}

/* Nombre de una nota MIDI: "C#3", "A4"... (C4 = 60) */
static inline void instr_nombre_nota(uint8_t nota, char *buf, size_t n)
{
    static const char *const nom[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
    snprintf(buf, n, "%s%d", nom[nota % 12], (int)(nota / 12) - 1);
}

static int instr_zonas_notas(const InstrumentoInfo_t *I, int *nota_z)
{
    int nz = I->zonas;
    if (nz > INSTR_MAX_ZONAS_BANCO) nz = INSTR_MAX_ZONAS_BANCO;
    for (int i = 0; i < nz; i++) nota_z[i] = instr_nota_de_zona(&I->banco[i]);
    return nz;
}

/* ──────────────────────────────────────────────────────────────
 *  NOTAS INICIALES SELECCIONABLES
 *  Llena 'out' (orden ascendente, notas MIDI) y devuelve la cantidad.
 * ────────────────────────────────────────────────────────────── */
static uint8_t instr_notas_inicio(uint8_t id, uint8_t *out, uint8_t max)
{
    if (id >= INSTR_CANTIDAD) id = 0;
    const InstrumentoInfo_t *I = &instrumentos_tabla[id];
    uint8_t n = 0;

    if (I->sintetico) {
        for (int nota = INSTR_SINT_NOTA_MIN;
             nota <= INSTR_SINT_NOTA_MAX - (INSTR_TECLAS - 1) && n < max; nota++) {
            out[n++] = (uint8_t)nota;
        }
        return n;
    }

    int nota_z[INSTR_MAX_ZONAS_BANCO];
    int nz = instr_zonas_notas(I, nota_z);

    int nmin = 127, nmax = 0;
    for (int i = 0; i < nz; i++) {
        if (nota_z[i] < nmin) nmin = nota_z[i];
        if (nota_z[i] > nmax) nmax = nota_z[i];
    }

    int primera = nmin - 1;                                  /* la más grave menos 1 semitono */
    int ultima  = (nmax + 1) - (INSTR_TECLAS - 1);
    
    if (primera < 0)       primera = 0;
    if (ultima < primera)  ultima = primera;

    if (n < max) out[n++] = (uint8_t)primera;

    int previa = primera;
    for (;;) {                                               /* notas originales, sin repetir */
        int sig = 1000;
        for (int i = 0; i < nz; i++) {
            if (nota_z[i] > previa && nota_z[i] < sig) sig = nota_z[i];
        }
        if (sig == 1000 || sig > ultima || n >= max) break;
        out[n++] = (uint8_t)sig;
        previa = sig;
    }
    return n;
}

/* La nota inicial válida más cercana a 'nota' para el instrumento 'id' */
static uint8_t instr_inicio_mas_cercano(uint8_t id, uint8_t nota)
{
    uint8_t lista[INSTR_MAX_INICIOS];
    uint8_t n = instr_notas_inicio(id, lista, INSTR_MAX_INICIOS);
    if (n == 0) return nota;

    uint8_t mejor = lista[0];
    int dmejor = abs((int)lista[0] - (int)nota);
    for (uint8_t i = 1; i < n; i++) {
        int d = abs((int)lista[i] - (int)nota);
        if (d < dmejor) { dmejor = d; mejor = lista[i]; }    /* empate: la más grave */
    }
    return mejor;
}

/* ──────────────────────────────────────────────────────────────
 *  PLAN DE ZONAS: qué zonas del banco copiar a RAM y qué zona usa cada tecla
 * ────────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t cantidad;                          /* zonas a copiar a RAM                 */
    uint8_t banco_idx[MAX_ZONAS_RAM];          /* índice en el banco (flash) de cada una */
    uint8_t zona_de_tecla[INSTR_TECLAS];       /* zona de RAM que usa cada tecla       */
    uint8_t max_desvio;                        /* mayor |semitonos| tecla vs. muestra  */
} PlanZonas_t;

/* Zona vigente más cercana en altura (empate: la primera = la más grave) */
static int instr_mas_cercana(const int *nota_z, const bool *vig, int nz, int nota)
{
    int mejor = -1, dmejor = 1000;
    for (int i = 0; i < nz; i++) {
        if (!vig[i]) continue;
        int d = abs(nota_z[i] - nota);
        if (d < dmejor) { dmejor = d; mejor = i; }
    }
    return mejor;
}

static void instr_planificar(uint8_t id, uint8_t nota_inicio, PlanZonas_t *plan)
{
    if (id >= INSTR_CANTIDAD) id = 0;
    const InstrumentoInfo_t *I = &instrumentos_tabla[id];
    plan->max_desvio = 0;

    /* ---- Básicos: zonas repartidas de grave a agudo sobre las 6 octavas ---- */
    if (I->sintetico) {
        int N = I->zonas < MAX_ZONAS_RAM ? I->zonas : MAX_ZONAS_RAM;
        if (N < 1) N = 1;
        plan->cantidad = (uint8_t)N;
        for (int j = 0; j < N; j++) {
            plan->banco_idx[j] = (uint8_t)((N > 1)
                ? (j * (I->zonas - 1) + (N - 1) / 2) / (N - 1) : 0);
        }
        const int rango = INSTR_SINT_NOTA_MAX - INSTR_SINT_NOTA_MIN + 1;
        for (int k = 0; k < INSTR_TECLAS; k++) {
            int nota = (int)nota_inicio + k;
            if (nota < INSTR_SINT_NOTA_MIN) nota = INSTR_SINT_NOTA_MIN;
            if (nota > INSTR_SINT_NOTA_MAX) nota = INSTR_SINT_NOTA_MAX;
            plan->zona_de_tecla[k] = (uint8_t)(((nota - INSTR_SINT_NOTA_MIN) * N) / rango);
        }
        return;
    }

    /* ---- Reales: cada tecla usa la muestra de la nota más cercana ---- */
    int  nota_z[INSTR_MAX_ZONAS_BANCO];
    bool vig[INSTR_MAX_ZONAS_BANCO];
    int  asig[INSTR_TECLAS];
    int  nz = instr_zonas_notas(I, nota_z);

    for (int i = 0; i < nz; i++) {                 /* notas repetidas (p. ej. dos Ds4): se usa la 1ª */
        vig[i] = true;
        for (int j = 0; j < i; j++) if (vig[j] && nota_z[j] == nota_z[i]) { vig[i] = false; break; }
    }

    for (;;) {
        bool usada[INSTR_MAX_ZONAS_BANCO] = { false };
        int  n_usadas = 0;
        for (int k = 0; k < INSTR_TECLAS; k++) {
            asig[k] = instr_mas_cercana(nota_z, vig, nz, (int)nota_inicio + k);
            if (!usada[asig[k]]) { usada[asig[k]] = true; n_usadas++; }
        }
        if (n_usadas <= MAX_ZONAS_RAM) break;

        /* No entran en RAM: sacar la zona cuya ausencia agrega menos error */
        int quitar = -1, costo_min = 1 << 30;
        for (int u = 0; u < nz; u++) {
            if (!usada[u]) continue;
            vig[u] = false;
            int costo = 0;
            for (int k = 0; k < INSTR_TECLAS; k++) {
                if (asig[k] != u) continue;
                int nota = (int)nota_inicio + k;
                int alt  = instr_mas_cercana(nota_z, vig, nz, nota);
                costo += abs(nota_z[alt] - nota) - abs(nota_z[u] - nota);
            }
            vig[u] = true;
            if (costo < costo_min) { costo_min = costo; quitar = u; }
        }
        vig[quitar] = false;
    }

    /* Zonas usadas -> casilleros de RAM (en el orden del banco) */
    int slot_de[INSTR_MAX_ZONAS_BANCO];
    uint8_t cant = 0;
    for (int i = 0; i < nz; i++) slot_de[i] = -1;                      /* -1 = no usada   */
    for (int k = 0; k < INSTR_TECLAS; k++) slot_de[asig[k]] = -2;      /* -2 = usada      */
    for (int i = 0; i < nz; i++) {
        if (slot_de[i] == -2) { slot_de[i] = cant; plan->banco_idx[cant] = (uint8_t)i; cant++; }
    }
    plan->cantidad = cant;

    for (int k = 0; k < INSTR_TECLAS; k++) {
        plan->zona_de_tecla[k] = (uint8_t)slot_de[asig[k]];
        int d = abs(nota_z[asig[k]] - ((int)nota_inicio + k));
        if (d > plan->max_desvio) plan->max_desvio = (uint8_t)d;
    }
}

#endif /* INSTRUMENTOS_H */