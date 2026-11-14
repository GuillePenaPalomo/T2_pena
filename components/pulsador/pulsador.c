/* pulsador.c - implementación del componente pulsador.
 *
 * Todo el acceso al pin del pulsador pasa por aquí, también la rutina de
 * interrupción y su tarea, desde el Taller 2. Quien usa el componente habla de
 * «pulsado»; qué nivel lógico es eso, y qué resistencia de polarización le
 * corresponde, lo resuelve este fichero con el dato del montaje.
 */

#include "pulsador.h"

#include <inttypes.h>
#include <stdio.h>

#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "pulsador";

esp_err_t pulsador_iniciar(pulsador_t *pulsador, gpio_num_t pin, int nivel_pulsado)
{
    if (pulsador == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (pin == GPIO_NUM_NC || !GPIO_IS_VALID_GPIO(pin))
    {
        ESP_LOGE(TAG, "el pin del pulsador no esta elegido o no existe");
        return ESP_ERR_INVALID_ARG;
    }

    pulsador->pin = pin;
    pulsador->nivel_pulsado = nivel_pulsado ? 1 : 0;
    pulsador->polarizado = true; 

    /* Con el pulsador a masa, el reposo es el nivel alto y lo da el pull-up;
     * pulsar lleva el pin a nivel bajo, y la interrupción se pide en el
     * flanco de bajada. Con el pulsador a 3,3 V, todo al revés. */
    const bool a_masa = (pulsador->nivel_pulsado == 0);
    const gpio_config_t configuracion = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = a_masa ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = a_masa ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = a_masa ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE,
    };

    const esp_err_t error = gpio_config(&configuracion);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "no se ha podido configurar el GPIO %d: %s",
                 (int)pin, esp_err_to_name(error));
        return error;
    }

    /* gpio_config() deja la interrupción del pin activada. Se desactiva hasta
     * que el programa le añada un manejador con gpio_isr_handler_add(), que
     * la vuelve a activar: así, mientras se lee el pin sin interrupción, un
     * pin flotante no dispara interrupciones que nadie atiende. */
    gpio_intr_disable(pin);

    ESP_LOGI(TAG, "pulsador en el GPIO %d, pulsado con nivel %d, %s interno",
             (int)pin, pulsador->nivel_pulsado, a_masa ? "pull-up" : "pull-down");
    return ESP_OK;
}

esp_err_t pulsador_polarizar(pulsador_t *pulsador, bool activa)
{
    gpio_pull_mode_t modo = GPIO_FLOATING;
    if (activa)
    {
        modo = (pulsador->nivel_pulsado == 0) ? GPIO_PULLUP_ONLY : GPIO_PULLDOWN_ONLY;
    }
    const esp_err_t error = gpio_set_pull_mode(pulsador->pin, modo);
    if (error == ESP_OK)
    {
        pulsador->polarizado = activa;
    }
    return error;
}

int pulsador_nivel(const pulsador_t *pulsador)
{
    return gpio_get_level(pulsador->pin);
}

bool pulsador_pulsado(const pulsador_t *pulsador)
{
    return gpio_get_level(pulsador->pin) == pulsador->nivel_pulsado;
}

/* ---- La atención por interrupción ------------------------------------------ */

#define LONGITUD_COLA 16

static pulsador_t *s_pulsador;
static pulsador_atencion_t s_atencion;
static QueueHandle_t s_flancos;
static volatile uint32_t s_pulsaciones;
static volatile uint32_t s_descartados;

/* La rutina de interrupción: anota el instante del flanco y lo manda a la
 * tarea por la cola. Nada más: ni trazas, ni esperas, ni LEDC. Si la cola
 * está llena, el flanco se pierde, y no importa: una ráfaga de rebotes que
 * la llena ya ha dejado en ella el primer flanco, que es el que cuenta. */
static void IRAM_ATTR isr_pulsador(void *argumento)
{
    (void)argumento;
    const int64_t instante = esp_timer_get_time();
    BaseType_t despertada = pdFALSE;
    xQueueSendFromISR(s_flancos, &instante, &despertada);
    portYIELD_FROM_ISR(despertada);
}

/* El antirrebote de U2-S2 y del Taller 1, con la confirmación del nivel:
 *   1. Un flanco que llega antes de que pase la guarda desde la última
 *      pulsación aceptada es rebote, y se descarta.
 *   2. Si llega después, la tarea espera a que pase el rebote y comprueba
 *      que el pin sigue pulsado. Sin esa comprobación, los rebotes al soltar
 *      una pulsación más larga que la guarda llegarían con la guarda ya
 *      vencida y contarían como una pulsación nueva.
 * Mientras espera, los flancos que llegan quedan en la cola y después los
 * descarta la guarda, que es más larga que la confirmación. */
static void tarea_pulsador(void *argumento)
{
    (void)argumento;
    const int64_t guarda_us = (int64_t)s_atencion.guarda_ms * 1000;
    int64_t ultima_us = INT64_MIN / 2; /* el primer flanco queda fuera de la guarda */

    for (;;)
    {
        int64_t flanco_us;
        xQueueReceive(s_flancos, &flanco_us, portMAX_DELAY);

        if (flanco_us - ultima_us < guarda_us)
        {
            s_descartados = s_descartados + 1;
            continue;
        }
        ultima_us = flanco_us;

        vTaskDelay(pdMS_TO_TICKS(s_atencion.confirmacion_ms));
        if (!pulsador_pulsado(s_pulsador))
        {
            /* Un rebote al soltar, o ruido: el pin ya no está pulsado. */
            s_descartados = s_descartados + 1;
            continue;
        }

        s_pulsaciones = s_pulsaciones + 1;
        printf("pulsador: pulsacion %" PRIu32 " (flancos descartados hasta ahora: %" PRIu32 ")\n",
               s_pulsaciones, s_descartados);
        if (s_atencion.aviso != NULL)
        {
            s_atencion.aviso(s_pulsaciones, s_atencion.argumento);
        }
    }
}

esp_err_t pulsador_atender(pulsador_t *pulsador, const pulsador_atencion_t *atencion)
{
    if (pulsador == NULL || atencion == NULL ||
        atencion->confirmacion_ms >= atencion->guarda_ms)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_flancos != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    s_pulsador = pulsador;
    s_atencion = *atencion;

    s_flancos = xQueueCreate(LONGITUD_COLA, sizeof(int64_t));
    if (s_flancos == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreatePinnedToCore(tarea_pulsador, "pulsador", atencion->pila, NULL,
                                atencion->prioridad, NULL, atencion->nucleo) != pdPASS)
    {
        return ESP_ERR_NO_MEM;
    }

    /* El servicio se instala una vez para todo el proyecto. Si ya lo instaló
     * otro, devuelve ESP_ERR_INVALID_STATE, y vale. */
    esp_err_t error = gpio_install_isr_service(0);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
    {
        return error;
    }
    error = gpio_isr_handler_add(pulsador->pin, isr_pulsador, NULL);
    if (error != ESP_OK)
    {
        return error;
    }

    ESP_LOGI(TAG, "atendido por interrupcion: guarda %" PRIu32 " ms, confirmacion %" PRIu32 " ms",
             atencion->guarda_ms, atencion->confirmacion_ms);
    return ESP_OK;
}

void pulsador_cuentas(uint32_t *pulsaciones, uint32_t *descartados)
{
    *pulsaciones = s_pulsaciones;
    *descartados = s_descartados;
}
