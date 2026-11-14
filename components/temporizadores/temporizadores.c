/* temporizadores.c - alarma de esp_timer frente a temporizador software. Ver
 * temporizadores.h. Sigue la variante 9 del ejemplo de la asignatura
 * SEyTR_planificacion_de_tareas (periodicas.c), con la medida en una tarea
 * aparte en lugar de dentro de las funciones de respuesta. */
#include "temporizadores.h"

#include <inttypes.h>
#include <stdio.h>

#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "temporizadores";

#define LONGITUD_COLA 32

typedef struct
{
    temporizador_t origen;
    int64_t instante_us;
} vencimiento_t;

static const char *const NOMBRES[TEMPORIZADOR_N] = {
    [TEMPORIZADOR_ALARMA] = "alarma esp_timer",
    [TEMPORIZADOR_SOFTWARE] = "temporizador software",
};

static temporizadores_config_t s_config;
static QueueHandle_t s_cola;
static esp_timer_handle_t s_alarma;
static TimerHandle_t s_software;
static volatile uint32_t s_periodo_ms;
static volatile uint32_t s_generacion_periodo;

/* Los escribe la función de respuesta de cada temporizador, una sola tarea
 * por contador. */
static volatile uint32_t s_perdidos[TEMPORIZADOR_N];

/* Lo que escribe la tarea de medida y leen los comandos de la consola. */
static portMUX_TYPE s_cerrojo = portMUX_INITIALIZER_UNLOCKED;
static bool s_hay[TEMPORIZADOR_N][CARGA_N_MODOS];
static resumen_datos_t s_ultimo[TEMPORIZADOR_N][CARGA_N_MODOS];
static uint32_t s_periodo_ultimo[TEMPORIZADOR_N][CARGA_N_MODOS]; /* con el que se midió */

/* La función de respuesta común. Se ejecuta en una tarea, la de esp_timer o
 * la de servicio de temporizadores, no en una rutina de interrupción: manda
 * con xQueueSend() y espera 0. Nunca se bloquea: si se bloqueara la del
 * temporizador software, se pararía la tarea de servicio y, con ella, todos
 * los temporizadores software del programa. */
static void anotar_vencimiento(temporizador_t origen)
{
    const vencimiento_t v = {.origen = origen, .instante_us = esp_timer_get_time()};
    if (xQueueSend(s_cola, &v, 0) != pdTRUE)
    {
        s_perdidos[origen] = s_perdidos[origen] + 1;
    }
}

static void respuesta_alarma(void *argumento)
{
    (void)argumento;
    anotar_vencimiento(TEMPORIZADOR_ALARMA);
}

static void respuesta_software(TimerHandle_t temporizador)
{
    (void)temporizador;
    anotar_vencimiento(TEMPORIZADOR_SOFTWARE);
}

static void tarea_medida(void *argumento)
{
    (void)argumento;
    resumen_t resumen[TEMPORIZADOR_N];
    uint32_t perdidos_vistos[TEMPORIZADOR_N];
    bool escrito[TEMPORIZADOR_N];
    for (int t = 0; t < TEMPORIZADOR_N; t++)
    {
        resumen_iniciar(&resumen[t], s_config.intervalos);
        perdidos_vistos[t] = s_perdidos[t];
        escrito[t] = false;
    }
    uint32_t generacion_carga = carga_generacion();
    uint32_t generacion_periodo = s_generacion_periodo;
    carga_modo_t modo = carga_modo();

    for (;;)
    {
        vencimiento_t v;
        xQueueReceive(s_cola, &v, portMAX_DELAY);

        /* Un resumen, un solo modo de la carga y un solo periodo. */
        const uint32_t gc = carga_generacion();
        const uint32_t gp = s_generacion_periodo;
        if (gc != generacion_carga || gp != generacion_periodo)
        {
            generacion_carga = gc;
            generacion_periodo = gp;
            modo = carga_modo();
            for (int t = 0; t < TEMPORIZADOR_N; t++)
            {
                resumen_reiniciar(&resumen[t]);
                escrito[t] = false;
            }
        }

        /* Un instante perdido junta dos intervalos en uno: la medida de ese
         * temporizador empieza de nuevo. */
        const temporizador_t t = v.origen;
        const uint32_t perdidos = s_perdidos[t];
        if (perdidos != perdidos_vistos[t])
        {
            perdidos_vistos[t] = perdidos;
            resumen_reiniciar(&resumen[t]);
        }

        resumen_datos_t d;
        if (!resumen_anotar(&resumen[t], v.instante_us, false, &d))
        {
            continue;
        }

        portENTER_CRITICAL(&s_cerrojo);
        s_ultimo[t][modo] = d;
        s_periodo_ultimo[t][modo] = s_periodo_ms;
        s_hay[t][modo] = true;
        portEXIT_CRITICAL(&s_cerrojo);

        /* Se escribe solo el primer resumen de cada medida: con 10 ms de
         * periodo sale uno cada medio segundo, y escribirlos todos llenaría
         * el monitor. El comando jitter da los últimos. */
        if (!escrito[t])
        {
            escrito[t] = true;
            char etiqueta[64];
            snprintf(etiqueta, sizeof etiqueta, "%s, carga %s", NOMBRES[t],
                     carga_nombre_modo(modo));
            resumen_escribir(etiqueta, &d, (int64_t)s_periodo_ms * 1000);
        }
    }
}

esp_err_t temporizadores_iniciar(const temporizadores_config_t *config)
{
    ESP_RETURN_ON_FALSE(config && config->periodo_ms > 0 && config->intervalos > 0,
                        ESP_ERR_INVALID_ARG, TAG, "configuracion no valida");
    ESP_RETURN_ON_FALSE(s_cola == NULL, ESP_ERR_INVALID_STATE, TAG, "ya iniciado");
    s_config = *config;
    s_periodo_ms = config->periodo_ms;

    s_cola = xQueueCreate(LONGITUD_COLA, sizeof(vencimiento_t));
    ESP_RETURN_ON_FALSE(s_cola, ESP_ERR_NO_MEM, TAG, "no hay memoria para la cola");
    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCore(tarea_medida, "medida", config->pila_medida,
                                                NULL, config->prioridad_medida, NULL,
                                                config->nucleo_medida) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "no hay memoria para la tarea de medida");

    const esp_timer_create_args_t argumentos = {
        .callback = respuesta_alarma,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK, /* la ejecuta la tarea de esp_timer */
        .name = "alarma",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&argumentos, &s_alarma), TAG, "esp_timer_create");

    s_software = xTimerCreate("software", pdMS_TO_TICKS(config->periodo_ms), pdTRUE, NULL,
                              respuesta_software);
    ESP_RETURN_ON_FALSE(s_software, ESP_ERR_NO_MEM, TAG, "no hay memoria para el temporizador");

    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_alarma, (uint64_t)config->periodo_ms * 1000),
                        TAG, "esp_timer_start_periodic");
    ESP_RETURN_ON_FALSE(xTimerStart(s_software, portMAX_DELAY) == pdPASS, ESP_FAIL, TAG,
                        "xTimerStart");

    ESP_LOGI(TAG, "alarma de esp_timer y temporizador software de %" PRIu32 " ms; un resumen "
                  "cada %" PRIu32 " intervalos",
             config->periodo_ms, config->intervalos);
    return ESP_OK;
}

esp_err_t temporizadores_poner_periodo(uint32_t periodo_ms)
{
    ESP_RETURN_ON_FALSE(s_alarma && s_software, ESP_ERR_INVALID_STATE, TAG, "no iniciado");
    ESP_RETURN_ON_FALSE(periodo_ms > 0 && pdMS_TO_TICKS(periodo_ms) > 0, ESP_ERR_INVALID_ARG,
                        TAG, "periodo no valido");

    ESP_RETURN_ON_ERROR(esp_timer_restart(s_alarma, (uint64_t)periodo_ms * 1000), TAG,
                        "esp_timer_restart");
    /* Manda la orden a la tarea de servicio. Con espera 0 no se bloquea: si
     * su cola estuviera llena, devolvería pdFAIL. */
    ESP_RETURN_ON_FALSE(xTimerChangePeriod(s_software, pdMS_TO_TICKS(periodo_ms), 0) == pdPASS,
                        ESP_FAIL, TAG, "xTimerChangePeriod");
    s_periodo_ms = periodo_ms;
    s_generacion_periodo = s_generacion_periodo + 1;
    return ESP_OK;
}

uint32_t temporizadores_periodo_ms(void)
{
    return s_periodo_ms;
}

bool temporizadores_ultimo_resumen(temporizador_t t, carga_modo_t modo, resumen_datos_t *d,
                                   uint32_t *periodo_ms)
{
    if ((unsigned)t >= TEMPORIZADOR_N || (unsigned)modo >= CARGA_N_MODOS)
    {
        return false;
    }
    portENTER_CRITICAL(&s_cerrojo);
    const bool hay = s_hay[t][modo];
    *d = s_ultimo[t][modo];
    *periodo_ms = s_periodo_ultimo[t][modo];
    portEXIT_CRITICAL(&s_cerrojo);
    return hay;
}

uint32_t temporizadores_perdidos(temporizador_t t)
{
    return (unsigned)t < TEMPORIZADOR_N ? s_perdidos[t] : 0;
}

const char *temporizadores_nombre(temporizador_t t)
{
    return (unsigned)t < TEMPORIZADOR_N ? NOMBRES[t] : "?";
}
