/* led.h - el brillo de un LED con una salida modulada de LEDC.
 *
 * Cabecera pública del componente. Es la práctica P3 del Taller 2: el LED del
 * Taller 1 deja de ser una salida digital y pasa a LEDC, el periférico de
 * modulación por ancho de pulso del ESP32-S3. Un temporizador de LEDC fija la
 * frecuencia y la resolución, y un canal lo saca por el pin del LED con un
 * ciclo de trabajo: la fracción de cada periodo en que la salida está alta.
 *
 * Una sola tarea, «led», maneja LEDC. Las demás le mandan órdenes por una
 * cola: el pulsador, «modo siguiente», y la consola, «modo» o «ciclo». Así
 * ninguna llamada a LEDC se hace desde una rutina de interrupción ni desde dos
 * tareas a la vez, y una orden que llega durante una transición no deja
 * esperando a quien la manda.
 *
 * Modos, en el orden en que se recorren:
 *   apagado     0 %
 *   medio       un nivel intermedio
 *   encendido   100 %
 *   transicion  el brillo sube y baja solo, por hardware
 * Además, «ciclo» fija un ciclo de trabajo cualquiera, en tanto por ciento.
 *
 * El componente no sabe en qué pin está el LED ni con qué prioridad corre su
 * tarea: los recibe al iniciarse.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LED_APAGADO,
        LED_MEDIO,
        LED_ENCENDIDO,
        LED_TRANSICION,
        LED_N_MODOS,  /* cuántos modos recorre el pulsador: no es un modo */
        LED_MANUAL = LED_N_MODOS, /* ciclo de trabajo fijado con led_poner_ciclo() */
    } led_modo_t;

    typedef struct
    {
        gpio_num_t pin;           /* GPIO del LED */
        int nivel_encendido;      /* 1 si el LED va entre el pin y masa; 0 si va a 3,3 V */
        uint32_t frecuencia_hz;   /* frecuencia de la señal modulada */
        uint32_t resolucion_bits; /* pasos por periodo: 2^resolucion_bits */
        uint32_t medio_porcentaje; /* el brillo del modo medio */
        uint32_t transicion_ms;   /* duración pedida de cada subida o bajada */
        UBaseType_t prioridad;    /* la de la tarea «led» */
        BaseType_t nucleo;
        uint32_t pila;            /* en bytes */
    } led_config_t;

    /* Configura el temporizador y el canal de LEDC, instala el servicio de
     * transiciones y crea la tarea «led». El LED empieza apagado.
     *
     * Devuelve ESP_ERR_INVALID_ARG si el pin no puede ser salida, y el error de
     * ledc_timer_config() si la frecuencia y la resolución no caben en el reloj
     * del temporizador: hace falta un reloj de al menos frecuencia ×
     * 2^resolución, y el más rápido de LEDC en el ESP32-S3 es de 80 MHz. */
    esp_err_t led_iniciar(const led_config_t *config);

    /* Órdenes a la tarea «led». No llaman a LEDC: dejan la orden en la cola y
     * vuelven enseguida. Devuelven ESP_ERR_TIMEOUT si la cola está llena. */
    esp_err_t led_siguiente_modo(void);
    esp_err_t led_poner_modo(led_modo_t modo);
    esp_err_t led_poner_ciclo(uint32_t porcentaje);

    /* El último modo y el último ciclo de trabajo que ha aplicado la tarea, en
     * pasos del temporizador, y el valor que es el 100 %. */
    void led_estado(led_modo_t *modo, uint32_t *ciclo, uint32_t *maximo);

    const char *led_nombre_modo(led_modo_t modo);
    bool led_modo_por_nombre(const char *nombre, led_modo_t *modo);

#ifdef __cplusplus
}
#endif
