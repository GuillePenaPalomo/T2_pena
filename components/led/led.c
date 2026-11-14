/* led.c - el brillo de un LED con una salida modulada de LEDC. Ver led.h.
 *
 * Todo el acceso a LEDC pasa por la tarea «led». Las funciones públicas solo
 * dejan órdenes en su cola. La estructura sigue la variante 6 del ejemplo de
 * la asignatura SEyTR_depuracion_de_tareas (salida_modulada.c).
 */

#include "led.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

static const char *TAG = "led";

#define MODO_LEDC LEDC_LOW_SPEED_MODE /* el único del ESP32-S3 */
#define TEMPORIZADOR LEDC_TIMER_0
#define CANAL LEDC_CHANNEL_0
#define LONGITUD_COLA 8

typedef enum
{
    ORDEN_SIGUIENTE,
    ORDEN_MODO,
    ORDEN_CICLO,
    ORDEN_FIN_TRANSICION,
} tipo_orden_t;

typedef struct
{
    tipo_orden_t tipo;
    uint32_t valor; /* el modo, el porcentaje o el ciclo al terminar la transición */
} orden_t;

static const char *const NOMBRES[] = {
    [LED_APAGADO] = "apagado",
    [LED_MEDIO] = "medio",
    [LED_ENCENDIDO] = "encendido",
    [LED_TRANSICION] = "transicion",
    [LED_MANUAL] = "manual",
};

static led_config_t s_config;
static QueueHandle_t s_ordenes;

/* Estado de la salida. Lo escribe solo la tarea «led»; los demás lo leen con
 * led_estado(), dentro de la sección crítica. */
static portMUX_TYPE s_cerrojo = portMUX_INITIALIZER_UNLOCKED;
static led_modo_t s_modo = LED_APAGADO;
static uint32_t s_ciclo = 0;
static uint32_t s_destino = 0; /* ciclo al que va la transición en curso */

/* Ciclo de trabajo del 100 %. Se escribe 2^resolución, la salida alta todo el
 * periodo: el registro del ciclo admite un valor más que la resolución. La
 * excepción es la resolución máxima del temporizador, 14 bits en el ESP32-S3:
 * con ella, 2^14 desborda el contador interno, y el máximo es 2^14 - 1. */
static uint32_t maximo(void)
{
    const uint32_t bits = s_config.resolucion_bits;
    return (bits >= SOC_LEDC_TIMER_BIT_WIDTH) ? (1u << bits) - 1u : 1u << bits;
}

static uint32_t ciclo_de(uint32_t porcentaje)
{
    return (uint32_t)((uint64_t)maximo() * porcentaje / 100u);
}

/* LEDC llama a esta función al terminar cada transición, desde su propia
 * rutina de interrupción. No toca LEDC: avisa a la tarea. */
static bool IRAM_ATTR fin_de_transicion(const ledc_cb_param_t *parametro, void *argumento)
{
    (void)argumento;
    BaseType_t despertada = pdFALSE;
    if (parametro->event == LEDC_FADE_END_EVT)
    {
        const orden_t orden = {.tipo = ORDEN_FIN_TRANSICION, .valor = parametro->duty};
        xQueueSendFromISR(s_ordenes, &orden, &despertada);
    }
    return despertada == pdTRUE;
}

static void anotar(led_modo_t modo, uint32_t ciclo)
{
    portENTER_CRITICAL(&s_cerrojo);
    s_modo = modo;
    s_ciclo = ciclo;
    portEXIT_CRITICAL(&s_cerrojo);
}

/* Fija un ciclo de trabajo. Antes detiene la transición que pueda haber en
 * curso: si no, ledc_set_duty_and_update() esperaría a que terminara. */
static void aplicar_ciclo(led_modo_t modo, uint32_t ciclo)
{
    ledc_fade_stop(MODO_LEDC, CANAL);
    ledc_set_duty_and_update(MODO_LEDC, CANAL, ciclo, 0);
    anotar(modo, ciclo);
}

/* Empieza una transición hasta «destino», sin esperar a que termine. El
 * hardware cambia el ciclo a saltos; el tiempo real puede apartarse del pedido
 * hasta la mitad o el doble, porque los saltos se calculan con enteros. */
static void empezar_transicion(uint32_t destino)
{
    s_destino = destino;
    ledc_set_fade_time_and_start(MODO_LEDC, CANAL, destino, s_config.transicion_ms,
                                 LEDC_FADE_NO_WAIT);
}

static void aplicar_modo(led_modo_t modo)
{
    switch (modo)
    {
    case LED_APAGADO:
        aplicar_ciclo(modo, 0);
        break;
    case LED_MEDIO:
        aplicar_ciclo(modo, ciclo_de(s_config.medio_porcentaje));
        break;
    case LED_ENCENDIDO:
        aplicar_ciclo(modo, maximo());
        break;
    case LED_TRANSICION:
        /* Desde el 100 %: la primera es una bajada, y se ve enseguida. */
        aplicar_ciclo(modo, maximo());
        empezar_transicion(0);
        break;
    default:
        return;
    }
    printf("led: modo %s, ciclo de trabajo %" PRIu32 " de %" PRIu32 "%s\n",
           NOMBRES[modo], s_ciclo, maximo(),
           modo == LED_TRANSICION ? ", sube y baja por hardware" : "");
}

static void tarea_led(void *argumento)
{
    (void)argumento;
    orden_t orden;
    for (;;)
    {
        xQueueReceive(s_ordenes, &orden, portMAX_DELAY);
        switch (orden.tipo)
        {
        case ORDEN_SIGUIENTE:
        {
            /* Desde el modo manual se vuelve al primero. */
            const led_modo_t siguiente =
                (s_modo >= LED_N_MODOS) ? LED_APAGADO : (led_modo_t)((s_modo + 1) % LED_N_MODOS);
            aplicar_modo(siguiente);
            break;
        }
        case ORDEN_MODO:
            aplicar_modo((led_modo_t)orden.valor);
            break;
        case ORDEN_CICLO:
            aplicar_ciclo(LED_MANUAL, ciclo_de(orden.valor));
            printf("led: ciclo de trabajo %" PRIu32 " %%, %" PRIu32 " de %" PRIu32 "\n",
                   orden.valor, s_ciclo, maximo());
            break;
        case ORDEN_FIN_TRANSICION:
            /* Un aviso atrasado, de una transición que se cortó, se ignora:
             * el ciclo no está en el destino de la transición en curso. */
            if (s_modo == LED_TRANSICION && orden.valor == s_destino)
            {
                empezar_transicion(s_destino == 0 ? maximo() : 0);
            }
            break;
        }
    }
}

esp_err_t led_iniciar(const led_config_t *config)
{
    ESP_RETURN_ON_FALSE(config && GPIO_IS_VALID_OUTPUT_GPIO(config->pin), ESP_ERR_INVALID_ARG,
                        TAG, "el pin del LED no esta elegido o no puede ser salida");
    ESP_RETURN_ON_FALSE(config->resolucion_bits >= 1 &&
                        config->resolucion_bits <= SOC_LEDC_TIMER_BIT_WIDTH &&
                        config->medio_porcentaje <= 100,
                        ESP_ERR_INVALID_ARG, TAG, "configuracion no valida");
    ESP_RETURN_ON_FALSE(s_ordenes == NULL, ESP_ERR_INVALID_STATE, TAG, "ya iniciado");
    s_config = *config;

    s_ordenes = xQueueCreate(LONGITUD_COLA, sizeof(orden_t));
    ESP_RETURN_ON_FALSE(s_ordenes, ESP_ERR_NO_MEM, TAG, "no hay memoria para la cola");

    const uint64_t reloj = (uint64_t)config->frecuencia_hz << config->resolucion_bits;
    ESP_LOGI(TAG, "%" PRIu32 " Hz con %" PRIu32 " bits piden un reloj de al menos %" PRIu64 " Hz",
             config->frecuencia_hz, config->resolucion_bits, reloj);

    const ledc_timer_config_t temporizador = {
        .speed_mode = MODO_LEDC,
        .duty_resolution = (ledc_timer_bit_t)config->resolucion_bits,
        .timer_num = TEMPORIZADOR,
        .freq_hz = config->frecuencia_hz,
        .clk_cfg = LEDC_AUTO_CLK, /* el driver elige un reloj que la pueda dar */
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&temporizador), TAG,
                        "la frecuencia y la resolucion no caben en el reloj del temporizador");

    /* El canal toma el pin del LED: desde aquí lo maneja LEDC, no GPIO. */
    const ledc_channel_config_t canal = {
        .gpio_num = config->pin,
        .speed_mode = MODO_LEDC,
        .channel = CANAL,
        .timer_sel = TEMPORIZADOR,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = (config->nivel_encendido == 0),
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&canal), TAG, "ledc_channel_config");

    /* Instala la rutina de interrupción de LEDC que usan las transiciones y
     * las funciones seguras entre tareas, como ledc_set_duty_and_update(). */
    ESP_RETURN_ON_ERROR(ledc_fade_func_install(0), TAG, "ledc_fade_func_install");
    ledc_cbs_t avisos = {.fade_cb = fin_de_transicion};
    ESP_RETURN_ON_ERROR(ledc_cb_register(MODO_LEDC, CANAL, &avisos, NULL), TAG,
                        "ledc_cb_register");

    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCore(tarea_led, "led", config->pila, NULL,
                                                config->prioridad, NULL,
                                                config->nucleo) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "no hay memoria para la tarea");

    ESP_LOGI(TAG, "LEDC en el GPIO %d: %" PRIu32 " Hz, %" PRIu32 " bits, 100 %% = %" PRIu32,
             (int)config->pin, config->frecuencia_hz, config->resolucion_bits, maximo());
    return ESP_OK;
}

static esp_err_t enviar(tipo_orden_t tipo, uint32_t valor)
{
    ESP_RETURN_ON_FALSE(s_ordenes, ESP_ERR_INVALID_STATE, TAG, "no iniciado");
    const orden_t orden = {.tipo = tipo, .valor = valor};
    return xQueueSend(s_ordenes, &orden, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t led_siguiente_modo(void)
{
    return enviar(ORDEN_SIGUIENTE, 0);
}

esp_err_t led_poner_modo(led_modo_t modo)
{
    ESP_RETURN_ON_FALSE((unsigned)modo < LED_N_MODOS, ESP_ERR_INVALID_ARG, TAG, "modo no valido");
    return enviar(ORDEN_MODO, (uint32_t)modo);
}

esp_err_t led_poner_ciclo(uint32_t porcentaje)
{
    ESP_RETURN_ON_FALSE(porcentaje <= 100, ESP_ERR_INVALID_ARG, TAG, "porcentaje fuera de rango");
    return enviar(ORDEN_CICLO, porcentaje);
}

void led_estado(led_modo_t *modo, uint32_t *ciclo, uint32_t *max)
{
    portENTER_CRITICAL(&s_cerrojo);
    *modo = s_modo;
    *ciclo = s_ciclo;
    portEXIT_CRITICAL(&s_cerrojo);
    *max = maximo();
}

const char *led_nombre_modo(led_modo_t modo)
{
    return (unsigned)modo <= LED_MANUAL ? NOMBRES[modo] : "?";
}

bool led_modo_por_nombre(const char *nombre, led_modo_t *modo)
{
    for (int i = 0; i < LED_N_MODOS; i++)
    {
        if (strcmp(nombre, NOMBRES[i]) == 0)
        {
            *modo = (led_modo_t)i;
            return true;
        }
    }
    return false;
}
