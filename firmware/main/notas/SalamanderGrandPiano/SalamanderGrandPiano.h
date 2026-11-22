#ifndef SALAMANDERGRANDPIANO_H
#define SALAMANDERGRANDPIANO_H

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
#include "SalamanderGrandPiano_A0v1.h"
#include "SalamanderGrandPiano_C1v1.h"
#include "SalamanderGrandPiano_Ds1v1.h"
#include "SalamanderGrandPiano_Fs1v1.h"
#include "SalamanderGrandPiano_A1v1.h"
#include "SalamanderGrandPiano_C2v1.h"
#include "SalamanderGrandPiano_Ds2v1.h"
#include "SalamanderGrandPiano_Fs2v1.h"
#include "SalamanderGrandPiano_A2v1.h"
#include "SalamanderGrandPiano_C3v1.h"
#include "SalamanderGrandPiano_Ds3v1.h"
#include "SalamanderGrandPiano_Fs3v1.h"
#include "SalamanderGrandPiano_A3v1.h"
#include "SalamanderGrandPiano_C4v1.h"
#include "SalamanderGrandPiano_Ds4v1.h"
#include "SalamanderGrandPiano_Fs4v1.h"
#include "SalamanderGrandPiano_A4v1.h"
#include "SalamanderGrandPiano_C5v1.h"
#include "SalamanderGrandPiano_Ds5v1.h"
#include "SalamanderGrandPiano_Fs5v1.h"
#include "SalamanderGrandPiano_A5v1.h"
#include "SalamanderGrandPiano_C6v1.h"
#include "SalamanderGrandPiano_Ds6v1.h"
#include "SalamanderGrandPiano_Fs6v1.h"
#include "SalamanderGrandPiano_A6v1.h"
#include "SalamanderGrandPiano_C7v1.h"
#include "SalamanderGrandPiano_Ds7v1.h"
#include "SalamanderGrandPiano_Fs7v1.h"
#include "SalamanderGrandPiano_A7v1.h"
#include "SalamanderGrandPiano_C8v1.h"

// === BANCO DE DATOS ORDENADO (De Grave a Agudo) ===
#define SALAMANDERGRANDPIANO_CANTIDAD_ZONAS 30
// Puntos de envolvente de cada nota (se recorren en duracion_s segundos)
#define SALAMANDERGRANDPIANO_ENV_LENGTH 512

// Respaldo: .h de notas generados sin duracion -> 0.0f (valor por defecto en el firmware)
#ifndef SALAMANDERGRANDPIANO_A0V1_DURACION_S
#define SALAMANDERGRANDPIANO_A0V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_C1V1_DURACION_S
#define SALAMANDERGRANDPIANO_C1V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_DS1V1_DURACION_S
#define SALAMANDERGRANDPIANO_DS1V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_FS1V1_DURACION_S
#define SALAMANDERGRANDPIANO_FS1V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_A1V1_DURACION_S
#define SALAMANDERGRANDPIANO_A1V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_C2V1_DURACION_S
#define SALAMANDERGRANDPIANO_C2V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_DS2V1_DURACION_S
#define SALAMANDERGRANDPIANO_DS2V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_FS2V1_DURACION_S
#define SALAMANDERGRANDPIANO_FS2V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_A2V1_DURACION_S
#define SALAMANDERGRANDPIANO_A2V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_C3V1_DURACION_S
#define SALAMANDERGRANDPIANO_C3V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_DS3V1_DURACION_S
#define SALAMANDERGRANDPIANO_DS3V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_FS3V1_DURACION_S
#define SALAMANDERGRANDPIANO_FS3V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_A3V1_DURACION_S
#define SALAMANDERGRANDPIANO_A3V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_C4V1_DURACION_S
#define SALAMANDERGRANDPIANO_C4V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_DS4V1_DURACION_S
#define SALAMANDERGRANDPIANO_DS4V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_FS4V1_DURACION_S
#define SALAMANDERGRANDPIANO_FS4V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_A4V1_DURACION_S
#define SALAMANDERGRANDPIANO_A4V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_C5V1_DURACION_S
#define SALAMANDERGRANDPIANO_C5V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_DS5V1_DURACION_S
#define SALAMANDERGRANDPIANO_DS5V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_FS5V1_DURACION_S
#define SALAMANDERGRANDPIANO_FS5V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_A5V1_DURACION_S
#define SALAMANDERGRANDPIANO_A5V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_C6V1_DURACION_S
#define SALAMANDERGRANDPIANO_C6V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_DS6V1_DURACION_S
#define SALAMANDERGRANDPIANO_DS6V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_FS6V1_DURACION_S
#define SALAMANDERGRANDPIANO_FS6V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_A6V1_DURACION_S
#define SALAMANDERGRANDPIANO_A6V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_C7V1_DURACION_S
#define SALAMANDERGRANDPIANO_C7V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_DS7V1_DURACION_S
#define SALAMANDERGRANDPIANO_DS7V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_FS7V1_DURACION_S
#define SALAMANDERGRANDPIANO_FS7V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_A7V1_DURACION_S
#define SALAMANDERGRANDPIANO_A7V1_DURACION_S 0.0f
#endif
#ifndef SALAMANDERGRANDPIANO_C8V1_DURACION_S
#define SALAMANDERGRANDPIANO_C8V1_DURACION_S 0.0f
#endif

const ZonaWavetable_t banco_SalamanderGrandPiano[SALAMANDERGRANDPIANO_CANTIDAD_ZONAS] = {
    {SalamanderGrandPiano_A0v1_wavetable, SalamanderGrandPiano_A0v1_envelope, 27.50f, 1, SALAMANDERGRANDPIANO_A0V1_DURACION_S},
    {SalamanderGrandPiano_C1v1_wavetable, SalamanderGrandPiano_C1v1_envelope, 32.70f, 4, SALAMANDERGRANDPIANO_C1V1_DURACION_S},
    {SalamanderGrandPiano_Ds1v1_wavetable, SalamanderGrandPiano_Ds1v1_envelope, 38.89f, 7, SALAMANDERGRANDPIANO_DS1V1_DURACION_S},
    {SalamanderGrandPiano_Fs1v1_wavetable, SalamanderGrandPiano_Fs1v1_envelope, 46.25f, 10, SALAMANDERGRANDPIANO_FS1V1_DURACION_S},
    {SalamanderGrandPiano_A1v1_wavetable, SalamanderGrandPiano_A1v1_envelope, 55.00f, 13, SALAMANDERGRANDPIANO_A1V1_DURACION_S},
    {SalamanderGrandPiano_C2v1_wavetable, SalamanderGrandPiano_C2v1_envelope, 65.41f, 16, SALAMANDERGRANDPIANO_C2V1_DURACION_S},
    {SalamanderGrandPiano_Ds2v1_wavetable, SalamanderGrandPiano_Ds2v1_envelope, 77.78f, 19, SALAMANDERGRANDPIANO_DS2V1_DURACION_S},
    {SalamanderGrandPiano_Fs2v1_wavetable, SalamanderGrandPiano_Fs2v1_envelope, 92.50f, 22, SALAMANDERGRANDPIANO_FS2V1_DURACION_S},
    {SalamanderGrandPiano_A2v1_wavetable, SalamanderGrandPiano_A2v1_envelope, 110.00f, 25, SALAMANDERGRANDPIANO_A2V1_DURACION_S},
    {SalamanderGrandPiano_C3v1_wavetable, SalamanderGrandPiano_C3v1_envelope, 130.81f, 28, SALAMANDERGRANDPIANO_C3V1_DURACION_S},
    {SalamanderGrandPiano_Ds3v1_wavetable, SalamanderGrandPiano_Ds3v1_envelope, 155.56f, 31, SALAMANDERGRANDPIANO_DS3V1_DURACION_S},
    {SalamanderGrandPiano_Fs3v1_wavetable, SalamanderGrandPiano_Fs3v1_envelope, 185.00f, 34, SALAMANDERGRANDPIANO_FS3V1_DURACION_S},
    {SalamanderGrandPiano_A3v1_wavetable, SalamanderGrandPiano_A3v1_envelope, 220.00f, 37, SALAMANDERGRANDPIANO_A3V1_DURACION_S},
    {SalamanderGrandPiano_C4v1_wavetable, SalamanderGrandPiano_C4v1_envelope, 261.63f, 40, SALAMANDERGRANDPIANO_C4V1_DURACION_S},
    {SalamanderGrandPiano_Ds4v1_wavetable, SalamanderGrandPiano_Ds4v1_envelope, 311.13f, 43, SALAMANDERGRANDPIANO_DS4V1_DURACION_S},
    {SalamanderGrandPiano_Fs4v1_wavetable, SalamanderGrandPiano_Fs4v1_envelope, 369.99f, 46, SALAMANDERGRANDPIANO_FS4V1_DURACION_S},
    {SalamanderGrandPiano_A4v1_wavetable, SalamanderGrandPiano_A4v1_envelope, 440.00f, 49, SALAMANDERGRANDPIANO_A4V1_DURACION_S},
    {SalamanderGrandPiano_C5v1_wavetable, SalamanderGrandPiano_C5v1_envelope, 523.25f, 52, SALAMANDERGRANDPIANO_C5V1_DURACION_S},
    {SalamanderGrandPiano_Ds5v1_wavetable, SalamanderGrandPiano_Ds5v1_envelope, 622.25f, 55, SALAMANDERGRANDPIANO_DS5V1_DURACION_S},
    {SalamanderGrandPiano_Fs5v1_wavetable, SalamanderGrandPiano_Fs5v1_envelope, 739.99f, 58, SALAMANDERGRANDPIANO_FS5V1_DURACION_S},
    {SalamanderGrandPiano_A5v1_wavetable, SalamanderGrandPiano_A5v1_envelope, 880.00f, 61, SALAMANDERGRANDPIANO_A5V1_DURACION_S},
    {SalamanderGrandPiano_C6v1_wavetable, SalamanderGrandPiano_C6v1_envelope, 1046.50f, 64, SALAMANDERGRANDPIANO_C6V1_DURACION_S},
    {SalamanderGrandPiano_Ds6v1_wavetable, SalamanderGrandPiano_Ds6v1_envelope, 1244.51f, 67, SALAMANDERGRANDPIANO_DS6V1_DURACION_S},
    {SalamanderGrandPiano_Fs6v1_wavetable, SalamanderGrandPiano_Fs6v1_envelope, 1479.98f, 70, SALAMANDERGRANDPIANO_FS6V1_DURACION_S},
    {SalamanderGrandPiano_A6v1_wavetable, SalamanderGrandPiano_A6v1_envelope, 1760.00f, 73, SALAMANDERGRANDPIANO_A6V1_DURACION_S},
    {SalamanderGrandPiano_C7v1_wavetable, SalamanderGrandPiano_C7v1_envelope, 2093.00f, 76, SALAMANDERGRANDPIANO_C7V1_DURACION_S},
    {SalamanderGrandPiano_Ds7v1_wavetable, SalamanderGrandPiano_Ds7v1_envelope, 2489.02f, 79, SALAMANDERGRANDPIANO_DS7V1_DURACION_S},
    {SalamanderGrandPiano_Fs7v1_wavetable, SalamanderGrandPiano_Fs7v1_envelope, 2959.96f, 82, SALAMANDERGRANDPIANO_FS7V1_DURACION_S},
    {SalamanderGrandPiano_A7v1_wavetable, SalamanderGrandPiano_A7v1_envelope, 3520.00f, 85, SALAMANDERGRANDPIANO_A7V1_DURACION_S},
    {SalamanderGrandPiano_C8v1_wavetable, SalamanderGrandPiano_C8v1_envelope, 4186.01f, 88, SALAMANDERGRANDPIANO_C8V1_DURACION_S}
};

#endif // SALAMANDERGRANDPIANO_H
