#ifndef GUITARRA_H
#define GUITARRA_H

#include <stdint.h>

// === ESTRUCTURA DE DATOS DEL MOTOR DE SINTESIS (compartida entre instrumentos) ===
#ifndef ZONA_WAVETABLE_T_DEFINED
#define ZONA_WAVETABLE_T_DEFINED
typedef struct {
    const int16_t* wavetable;
    const uint16_t* envolvente;
    float frecuencia_base;
    uint16_t indice_tecla;
    float duracion_s;          // tiempo que abarca la envolvente (0 = usar valor por defecto)
} ZonaWavetable_t;
#endif

// === INCLUDES DE NOTAS INDIVIDUALES ===
#include "guitarra_F2v1.h"
#include "guitarra_Gs2v1.h"
#include "guitarra_B2v1.h"
#include "guitarra_D3v1.h"
#include "guitarra_F3v1.h"
#include "guitarra_Gs3v1.h"
#include "guitarra_B3v1.h"
#include "guitarra_D4v1.h"
#include "guitarra_F4v1.h"
#include "guitarra_Gs4v1.h"
#include "guitarra_B4v1.h"
#include "guitarra_E5v1.h"

// === BANCO DE DATOS ORDENADO (De Grave a Agudo) ===
#define GUITARRA_CANTIDAD_ZONAS 12
// Puntos de envolvente de cada nota (se recorren en duracion_s segundos)
#define GUITARRA_ENV_LENGTH 512

// Respaldo: .h de notas generados sin duracion -> 0.0f (valor por defecto en el firmware)
#ifndef GUITARRA_F2V1_DURACION_S
#define GUITARRA_F2V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_GS2V1_DURACION_S
#define GUITARRA_GS2V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_B2V1_DURACION_S
#define GUITARRA_B2V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_D3V1_DURACION_S
#define GUITARRA_D3V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_F3V1_DURACION_S
#define GUITARRA_F3V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_GS3V1_DURACION_S
#define GUITARRA_GS3V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_B3V1_DURACION_S
#define GUITARRA_B3V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_D4V1_DURACION_S
#define GUITARRA_D4V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_F4V1_DURACION_S
#define GUITARRA_F4V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_GS4V1_DURACION_S
#define GUITARRA_GS4V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_B4V1_DURACION_S
#define GUITARRA_B4V1_DURACION_S 0.0f
#endif
#ifndef GUITARRA_E5V1_DURACION_S
#define GUITARRA_E5V1_DURACION_S 0.0f
#endif

const ZonaWavetable_t banco_guitarra[GUITARRA_CANTIDAD_ZONAS] = {
    {guitarra_F2v1_wavetable, guitarra_F2v1_envelope, 87.31f, 1, GUITARRA_F2V1_DURACION_S},
    {guitarra_Gs2v1_wavetable, guitarra_Gs2v1_envelope, 103.83f, 4, GUITARRA_GS2V1_DURACION_S},
    {guitarra_B2v1_wavetable, guitarra_B2v1_envelope, 123.47f, 7, GUITARRA_B2V1_DURACION_S},
    {guitarra_D3v1_wavetable, guitarra_D3v1_envelope, 146.83f, 10, GUITARRA_D3V1_DURACION_S},
    {guitarra_F3v1_wavetable, guitarra_F3v1_envelope, 174.61f, 13, GUITARRA_F3V1_DURACION_S},
    {guitarra_Gs3v1_wavetable, guitarra_Gs3v1_envelope, 207.65f, 16, GUITARRA_GS3V1_DURACION_S},
    {guitarra_B3v1_wavetable, guitarra_B3v1_envelope, 246.94f, 19, GUITARRA_B3V1_DURACION_S},
    {guitarra_D4v1_wavetable, guitarra_D4v1_envelope, 293.66f, 22, GUITARRA_D4V1_DURACION_S},
    {guitarra_F4v1_wavetable, guitarra_F4v1_envelope, 349.23f, 25, GUITARRA_F4V1_DURACION_S},
    {guitarra_Gs4v1_wavetable, guitarra_Gs4v1_envelope, 415.30f, 28, GUITARRA_GS4V1_DURACION_S},
    {guitarra_B4v1_wavetable, guitarra_B4v1_envelope, 493.88f, 31, GUITARRA_B4V1_DURACION_S},
    {guitarra_E5v1_wavetable, guitarra_E5v1_envelope, 659.26f, 34, GUITARRA_E5V1_DURACION_S}
};

#endif // GUITARRA_H
