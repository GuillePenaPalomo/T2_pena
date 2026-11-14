/* pulsador.h - un pulsador en un pin de entrada digital, atendido por
 * interrupción.
 *
 * Cabecera pública del componente. Como el del LED, el componente no sabe en
 * qué pin está el pulsador ni cómo está montado: se lo dice quien lo inicia.
 *
 * Dos partes:
 *   - El pin: lo configura como entrada, activa o quita la resistencia de
 *     polarización interna y lee el nivel. Es lo que tenía el Taller 1.
 *   - La atención, añadida en el Taller 2 para P3: una rutina de interrupción
 *     mínima, que solo anota el instante del flanco y lo manda por una cola, y
 *     una tarea, «pulsador», que aplica el antirrebote y, por cada pulsación
 *     válida, llama a la función de aviso que le dé quien lo usa. El
 *     componente no sabe qué hace esa función: en el taller, pasar el LED al
 *     modo siguiente.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Estado de un pulsador. Se declara en quien lo usa y se pasa por puntero. */
typedef struct {
    gpio_num_t pin;            /* GPIO del pulsador */
    int        nivel_pulsado;  /* nivel lógico que se lee con el pulsador apretado: 0 o 1 */
    bool       polarizado;     /* si la resistencia de polarización interna está activada */
} pulsador_t;

/* Lo que hace el programa con cada pulsación válida. Se llama desde la tarea
 * «pulsador», no desde la interrupción: puede llamar a cualquier función que
 * se pueda llamar desde una tarea, pero no debe bloquearse mucho rato. */
typedef void (*pulsador_aviso_t)(uint32_t pulsaciones, void *argumento);

typedef struct {
    uint32_t guarda_ms;        /* tras una pulsación aceptada, lo que llegue antes es rebote */
    uint32_t confirmacion_ms;  /* espera antes de comprobar que el pin sigue pulsado */
    UBaseType_t prioridad;     /* la de la tarea «pulsador» */
    BaseType_t nucleo;
    uint32_t pila;             /* en bytes */
    pulsador_aviso_t aviso;
    void *argumento;           /* se pasa tal cual a aviso */
} pulsador_atencion_t;

/* Configura el pin como entrada, con la resistencia de polarización interna
 * que corresponde al montaje activada, y con la interrupción por el flanco que
 * va hacia el nivel de pulsado. La interrupción queda desactivada hasta que se
 * le añade un manejador, lo que hace pulsador_atender().
 *
 * pin: el GPIO del pulsador. Tiene que ser un pin válido: sin pulsador no hay
 *      nada que medir, y GPIO_NUM_NC da error.
 * nivel_pulsado: 0 si el pulsador va entre el pin y masa, con pull-up y lógica
 *      invertida; 1 si va entre el pin y 3,3 V, con pull-down.
 *
 * Devuelve ESP_OK; ESP_ERR_INVALID_ARG si pulsador es NULL o si el pin no
 * existe; o el error que devuelva gpio_config().
 */
esp_err_t pulsador_iniciar(pulsador_t *pulsador, gpio_num_t pin, int nivel_pulsado);

/* Activa o quita la resistencia de polarización interna. Sin ella, y sin una
 * externa, el pin queda flotante y su lectura no está determinada. */
esp_err_t pulsador_polarizar(pulsador_t *pulsador, bool activa);

/* Nivel lógico del pin, 0 o 1, tal como lo da gpio_get_level(). */
int pulsador_nivel(const pulsador_t *pulsador);

/* true si el nivel del pin es el de pulsado. */
bool pulsador_pulsado(const pulsador_t *pulsador);

/* Instala el servicio de interrupciones de GPIO si no lo está, crea la cola y
 * la tarea «pulsador» y registra la rutina de interrupción en el pin. Solo
 * admite un pulsador atendido. La confirmación tiene que ser menor que la
 * guarda. */
esp_err_t pulsador_atender(pulsador_t *pulsador, const pulsador_atencion_t *atencion);

/* Pulsaciones válidas y flancos descartados como rebote desde el arranque. */
void pulsador_cuentas(uint32_t *pulsaciones, uint32_t *descartados);

#ifdef __cplusplus
}
#endif
