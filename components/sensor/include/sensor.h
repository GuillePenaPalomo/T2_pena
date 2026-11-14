/* sensor.h - la tarea periódica que lee el BME280 y mide su propio periodo.
 *
 * Es la práctica P2 del Taller 2. Una tarea, «sensor», fijada al núcleo de la
 * carga, espera con xTaskDelayUntil(), una espera absoluta: hasta un instante
 * fijo, que avanza un periodo en cada vuelta, y no una duración. Así lo que
 * tarda cada vuelta no se suma al periodo.
 *
 * En cada activación toma el instante con esp_timer_get_time() nada más
 * volver de la espera, antes de leer el sensor, y lo anota en un resumen de
 * un número fijo de intervalos (componente resumen). Cuando el resumen se
 * completa, lo escribe y lo guarda como el último del modo de la carga con el
 * que se ha medido. Si el modo de la carga cambia a mitad, descarta lo
 * acumulado y empieza de nuevo: un resumen nunca mezcla dos modos.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "esp_err.h"

#include "bme280.h"
#include "carga.h"
#include "resumen.h"

typedef struct
{
    bme280_t *bme280;          /* ya configurado con bme280_configurar() */
    uint32_t periodo_ms;
    uint32_t intervalos;       /* intervalos de cada resumen */
    UBaseType_t prioridad;
    BaseType_t nucleo;
    uint32_t pila;             /* en bytes */
} sensor_config_t;

/* Crea la tarea del sensor. */
esp_err_t sensor_iniciar(const sensor_config_t *config);

/* Copia la última medida. Devuelve false si todavía no hay ninguna. */
bool sensor_ultima_medida(bme280_medida_t *m);

/* Copia el último resumen completo medido con el modo de carga dado.
 * Devuelve false si con ese modo no se ha completado ninguno. */
bool sensor_ultimo_resumen(carga_modo_t modo, resumen_datos_t *d, UBaseType_t *prioridad);

/* Intervalos que lleva el resumen en curso, y el total que lo completa. */
void sensor_progreso(uint32_t *llevados, uint32_t *total);

uint32_t sensor_periodo_ms(void);

/* Cambia la prioridad de la tarea del sensor y empieza la medida de nuevo.
 * Sirve para la extensión de P2: invertir la relación con la carga. */
esp_err_t sensor_poner_prioridad(UBaseType_t prioridad);
UBaseType_t sensor_prioridad(void);
