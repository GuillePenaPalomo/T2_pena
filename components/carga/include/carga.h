/* carga.h - dos tareas de cálculo que cargan un núcleo, y su informe.
 *
 * Es la práctica P1 del Taller 2. Dos tareas, «carga_a» y «carga_b», fijadas
 * al mismo núcleo, repiten un ciclo: una ráfaga de cálculo y una espera
 * bloqueada más larga que la ráfaga. Cada una cuenta el trabajo que hace, y
 * una tercera tarea, «informe», fijada al otro núcleo, escribe cada segundo
 * cuánto ha avanzado cada contador.
 *
 * La carga tiene cuatro modos, que se cambian en ejecución:
 *   parada     las dos tareas bloqueadas, sin calcular;
 *   turnos     las dos con la misma prioridad: el planificador las turna en
 *              cada tick y los dos contadores avanzan;
 *   inanicion  «carga_a» sube de prioridad y deja de bloquearse: «carga_b»
 *              no vuelve a recibir procesador, y tampoco la tarea inactiva
 *              del núcleo, así que a los 5 s avisa el watchdog de tareas;
 *   corregida  «carga_a» sigue con más prioridad, pero recupera su espera:
 *              mientras está bloqueada se ejecutan «carga_b» y la inactiva.
 *
 * Las otras prácticas usan estas tareas para medir con carga: paran la carga
 * para medir sin ella y la ponen en turnos para medir con ella. Para no
 * mezclar en una medida activaciones de dos modos, consultan carga_modo() y
 * carga_generacion().
 *
 * El componente no decide prioridades ni núcleos: los recibe al iniciarse.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "esp_err.h"

typedef enum
{
    CARGA_PARADA,
    CARGA_TURNOS,
    CARGA_INANICION,
    CARGA_CORREGIDA,
    CARGA_N_MODOS, /* cuántos modos hay: no es un modo */
} carga_modo_t;

typedef struct
{
    uint32_t rafaga_ms;            /* cálculo de cada ciclo, en tiempo de reloj */
    uint32_t espera_ms;            /* espera bloqueada de cada ciclo: mayor que la ráfaga */
    UBaseType_t prioridad;         /* la de las dos tareas de carga en turnos */
    UBaseType_t prioridad_alta;    /* la de «carga_a» en inanición y corregida */
    BaseType_t nucleo;             /* el núcleo de las dos tareas de carga */
    uint32_t pila;                 /* pila de cada tarea de carga, en bytes */
    UBaseType_t prioridad_informe; /* la tarea «informe» */
    BaseType_t nucleo_informe;     /* otro núcleo, para que siga escribiendo */
    uint32_t pila_informe;
} carga_config_t;

/* Crea las tres tareas. La carga empieza parada.
 *
 * Devuelve ESP_ERR_INVALID_ARG si la configuración no es válida: la espera
 * tiene que ser mayor que la ráfaga, para que la tarea inactiva llegue a
 * ejecutarse en turnos, y prioridad_alta mayor que prioridad. ESP_ERR_NO_MEM
 * si no hay memoria para las tareas.
 */
esp_err_t carga_iniciar(const carga_config_t *config);

/* Cambia el modo de la carga. Lo puede llamar cualquier tarea. */
esp_err_t carga_poner_modo(carga_modo_t modo);

carga_modo_t carga_modo(void);

/* Número que cambia cada vez que cambia el modo, también si se vuelve a uno
 * anterior. Quien mide lo compara con el que vio al empezar su medida. */
uint32_t carga_generacion(void);

/* Duración del ciclo, ráfaga más espera, en milisegundos. */
uint32_t carga_ciclo_ms(void);

/* Nombre de un modo, y el modo que corresponde a un nombre. */
const char *carga_nombre_modo(carga_modo_t modo);
bool carga_modo_por_nombre(const char *nombre, carga_modo_t *modo);

/* Activa o calla el informe de cada segundo. Con la carga parada no hay nada
 * que informar y no se escribe. */
void carga_informar(bool activar);
bool carga_informa(void);
