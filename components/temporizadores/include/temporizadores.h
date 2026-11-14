/* temporizadores.h - una alarma de esp_timer y un temporizador software de
 * FreeRTOS con el mismo periodo, y la medida de su jitter.
 *
 * Es la práctica P4 del Taller 2: dos formas de hacer algo periódico sin una
 * tarea que espere.
 *   - La alarma periódica de esp_timer cuenta en microsegundos. Su función de
 *     respuesta la ejecuta la tarea de esp_timer, de prioridad 22, fijada al
 *     núcleo 0: expulsa a la carga en cuanto vence.
 *   - El temporizador software de FreeRTOS cuenta en ticks. Su función de
 *     respuesta la ejecuta la tarea de servicio de temporizadores, «Tmr Svc»,
 *     de prioridad 1, fijada al núcleo 0 en sdkconfig.defaults: espera a que
 *     las tareas de carga, de prioridad 2, se bloqueen.
 *
 * Las dos funciones de respuesta son cortas y no se bloquean: toman el
 * instante con esp_timer_get_time() y lo mandan, con su origen, a la tarea
 * «medida» por una cola, sin esperar. La tarea de medida, en el otro núcleo,
 * calcula con el componente resumen un resumen por temporizador y empieza la
 * medida de nuevo cuando cambia el modo de la carga, cuando cambia el periodo
 * o cuando se pierde un instante porque la cola estaba llena: un instante
 * perdido juntaría dos intervalos en uno.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "esp_err.h"

#include "carga.h"
#include "resumen.h"

typedef enum
{
    TEMPORIZADOR_ALARMA,   /* esp_timer */
    TEMPORIZADOR_SOFTWARE, /* temporizador software de FreeRTOS */
    TEMPORIZADOR_N,
} temporizador_t;

typedef struct
{
    uint32_t periodo_ms;          /* el mismo para los dos */
    uint32_t intervalos;          /* de cada resumen */
    UBaseType_t prioridad_medida; /* la de la tarea «medida» */
    BaseType_t nucleo_medida;     /* el otro núcleo, o más prioridad que la carga */
    uint32_t pila_medida;         /* en bytes */
} temporizadores_config_t;

/* Crea la cola y la tarea de medida, la alarma y el temporizador software, y
 * los arranca. */
esp_err_t temporizadores_iniciar(const temporizadores_config_t *config);

/* Cambia el periodo de los dos: la alarma con esp_timer_restart(), que en una
 * alarma periódica fija el periodo nuevo, y el temporizador software con
 * xTimerChangePeriod(). La medida de los dos empieza de nuevo. */
esp_err_t temporizadores_poner_periodo(uint32_t periodo_ms);
uint32_t temporizadores_periodo_ms(void);

/* El último resumen completo de un temporizador con un modo de la carga, y el
 * periodo con que se midió. */
bool temporizadores_ultimo_resumen(temporizador_t t, carga_modo_t modo, resumen_datos_t *d,
                                   uint32_t *periodo_ms);

/* Envíos que han fallado porque la cola estaba llena, desde el arranque. */
uint32_t temporizadores_perdidos(temporizador_t t);

const char *temporizadores_nombre(temporizador_t t);
