/* resumen.h - resumen del periodo de una actividad periódica.
 *
 * Quien mide anota el instante de cada activación, en microsegundos, tomado
 * justo al despertar y antes de hacer nada más. El resumen calcula, sobre un
 * número fijo de intervalos entre activaciones seguidas:
 *   - el intervalo medio, el mínimo y el máximo;
 *   - el jitter, la diferencia entre el máximo y el mínimo;
 *   - los rebases, las activaciones que quien mide marca como tardías. En una
 *     tarea periódica, las que xTaskDelayUntil() señala devolviendo pdFALSE.
 *
 * Solo escribe en la memoria del resumen: no traza ni se bloquea, así que
 * medir no altera lo que se mide. Lo usan la tarea del sensor (P2) y la tarea
 * de medida de los temporizadores (P4).
 *
 * No depende de ESP-IDF ni de FreeRTOS: es C estándar y se puede probar en el
 * ordenador. Un resumen lo usa una sola tarea: no se protege.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Lo que sale de un resumen completo. */
typedef struct
{
    uint32_t n;         /* intervalos medidos */
    int64_t medio_us;
    int64_t min_us;
    int64_t max_us;
    int64_t jitter_us;  /* max_us - min_us */
    uint32_t rebases;
} resumen_datos_t;

typedef struct
{
    uint32_t objetivo;  /* intervalos que forman un resumen */
    uint32_t n;
    int64_t anterior_us; /* instante de la activación anterior, o -1 */
    int64_t suma_us;
    int64_t min_us;
    int64_t max_us;
    uint32_t rebases;
} resumen_t;

/* Prepara un resumen de «objetivo» intervalos, es decir, de objetivo + 1
 * activaciones. */
void resumen_iniciar(resumen_t *r, uint32_t objetivo);

/* Descarta todo lo acumulado, también la activación anterior: la siguiente
 * activación no forma intervalo, solo marca el comienzo. Se usa cuando cambia
 * algo que invalida la medida, como el modo de la carga o el periodo. */
void resumen_reiniciar(resumen_t *r);

/* Anota una activación. «tardia» dice si fue un rebase del periodo. Devuelve
 * true cuando el resumen se completa; entonces copia el resultado en «datos»
 * y empieza otro, que toma esta activación como comienzo. */
bool resumen_anotar(resumen_t *r, int64_t instante_us, bool tardia, resumen_datos_t *datos);

/* Intervalos que lleva el resumen en curso. */
uint32_t resumen_llevados(const resumen_t *r);

/* Escribe un resumen en una línea, en milisegundos con tres decimales, con
 * el periodo nominal al lado para compararlo. */
void resumen_escribir(const char *etiqueta, const resumen_datos_t *d, int64_t nominal_us);
