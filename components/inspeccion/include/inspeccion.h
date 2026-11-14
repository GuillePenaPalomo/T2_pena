/* inspeccion.h - la lista de tareas, el reparto del procesador y la pila de
 * cada tarea propia, escritos por el programa.
 *
 * Es la parte de la integración del Taller 2 que no escribe la pareja sino el
 * programa. inspeccion_escribir() escribe:
 *   - la salida de vTaskList(): estado, prioridad, pila mínima libre, número
 *     y núcleo de cada tarea;
 *   - la salida de vTaskGetRunTimeStats(): el tiempo de procesador de cada
 *     tarea desde el arranque;
 *   - la carga de cada núcleo desde la llamada anterior, que es el 100 %
 *     menos lo que se ha ejecutado su tarea inactiva en ese intervalo. Así se
 *     ve la carga de un modo sin la mezcla de todo lo anterior;
 *   - de cada tarea propia, la pila asignada y la marca de agua medida con
 *     uxTaskGetStackHighWaterMark(), que en ESP-IDF da bytes.
 *
 * Las funciones de inspección exigen las opciones de sdkconfig.defaults
 * CONFIG_FREERTOS_USE_TRACE_FACILITY, ..._USE_STATS_FORMATTING_FUNCTIONS,
 * ..._GENERATE_RUN_TIME_STATS y ..._VTASKLIST_INCLUDE_COREID.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct
{
    const char *nombre; /* el que se le dio al crearla */
    uint32_t pila;      /* asignada, en bytes */
} inspeccion_tarea_t;

/* Guarda la tabla de tareas propias. La tabla tiene que vivir siempre: se
 * guarda el puntero. */
esp_err_t inspeccion_iniciar(const inspeccion_tarea_t *tabla, uint32_t n);

/* Escribe todo lo anterior. Usa un búfer estático: no la pueden llamar dos
 * tareas a la vez. En este programa solo la llama la consola. */
void inspeccion_escribir(void);
