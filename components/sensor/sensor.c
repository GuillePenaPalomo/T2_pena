/* sensor.c - la tarea periódica que lee el BME280 y mide su periodo. Ver sensor.h. */
#include "sensor.h"

#include <inttypes.h>
#include <stdio.h>

#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "sensor";

static sensor_config_t s_config;
static TaskHandle_t s_tarea;

/* Lo que escribe la tarea del sensor y leen los comandos de la consola. Se lee
 * y se escribe dentro de la sección crítica: una medida o un resumen son
 * varios campos, y sin ella se podría leer la mitad de uno nuevo y la mitad
 * del anterior. */
static portMUX_TYPE s_cerrojo = portMUX_INITIALIZER_UNLOCKED;
static bool s_hay_medida;
static bme280_medida_t s_medida;
static bool s_hay_resumen[CARGA_N_MODOS];
static resumen_datos_t s_resumen[CARGA_N_MODOS];
static UBaseType_t s_prioridad_resumen[CARGA_N_MODOS];
static uint32_t s_llevados;

/* Cambia cuando cambia la prioridad: la tarea empieza la medida de nuevo. */
static volatile uint32_t s_generacion_prioridad;

static void escribir_medida(const bme280_medida_t *m)
{
    const int32_t t = m->temperatura_centigrados;
    const uint32_t ta = (uint32_t)(t < 0 ? -t : t);
    printf("sensor: T=%s%" PRIu32 ",%02" PRIu32 " C, P=%" PRIu32 ",%02" PRIu32
           " hPa, HR=%" PRIu32 ",%02" PRIu32 " %%\n",
           t < 0 ? "-" : "", ta / 100, ta % 100,
           m->presion_pa / 100, m->presion_pa % 100,
           m->humedad_centiporcentaje / 100, m->humedad_centiporcentaje % 100);
}

static void tarea_sensor(void *argumento)
{
    (void)argumento;
    resumen_t resumen;
    resumen_iniciar(&resumen, s_config.intervalos);

    uint32_t generacion_carga = carga_generacion();
    uint32_t generacion_prioridad = s_generacion_prioridad;
    carga_modo_t modo = carga_modo();
    uint32_t errores = 0;

    const TickType_t periodo = pdMS_TO_TICKS(s_config.periodo_ms);
    TickType_t referencia = xTaskGetTickCount();

    for (;;)
    {
        /* Espera absoluta. Devuelve pdFALSE si no ha tenido que esperar
         * porque el instante siguiente ya había pasado: la vuelta anterior
         * terminó tarde, y eso es un rebase del periodo. */
        const bool tardia = (xTaskDelayUntil(&referencia, periodo) == pdFALSE);
        /* El instante, lo primero: antes de leer el sensor, para que el
         * intervalo no incluya lo que varía la lectura. */
        const int64_t ahora = esp_timer_get_time();

        /* Un resumen, un solo modo de la carga y una sola prioridad. */
        const uint32_t gc = carga_generacion();
        const uint32_t gp = s_generacion_prioridad;
        if (gc != generacion_carga || gp != generacion_prioridad)
        {
            generacion_carga = gc;
            generacion_prioridad = gp;
            modo = carga_modo();
            resumen_reiniciar(&resumen);
        }

        resumen_datos_t datos;
        const bool completo = resumen_anotar(&resumen, ahora, tardia, &datos);

        bme280_medida_t medida;
        const esp_err_t err = bme280_leer(s_config.bme280, &medida);

        portENTER_CRITICAL(&s_cerrojo);
        if (err == ESP_OK)
        {
            s_medida = medida;
            s_hay_medida = true;
        }
        if (completo)
        {
            s_resumen[modo] = datos;
            s_hay_resumen[modo] = true;
            s_prioridad_resumen[modo] = uxTaskPriorityGet(NULL);
        }
        s_llevados = resumen_llevados(&resumen);
        portEXIT_CRITICAL(&s_cerrojo);

        if (err != ESP_OK && errores++ % 10 == 0)
        {
            ESP_LOGE(TAG, "no se pudo leer el sensor: %s", esp_err_to_name(err));
        }

        /* Solo se escribe el resumen, una vez cada «intervalos» vueltas, y
         * después de tomar el instante y leer: no altera lo que se mide. */
        if (completo)
        {
            char etiqueta[64];
            snprintf(etiqueta, sizeof etiqueta, "sensor, carga %s, prioridad %u",
                     carga_nombre_modo(modo), (unsigned)uxTaskPriorityGet(NULL));
            resumen_escribir(etiqueta, &datos, (int64_t)s_config.periodo_ms * 1000);
            if (err == ESP_OK)
            {
                escribir_medida(&medida);
            }
        }
    }
}

esp_err_t sensor_iniciar(const sensor_config_t *config)
{
    ESP_RETURN_ON_FALSE(config && config->bme280 && config->periodo_ms > 0 &&
                        config->intervalos > 0, ESP_ERR_INVALID_ARG, TAG,
                        "configuracion no valida");
    ESP_RETURN_ON_FALSE(s_tarea == NULL, ESP_ERR_INVALID_STATE, TAG, "ya iniciado");
    s_config = *config;

    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCore(tarea_sensor, "sensor", config->pila, NULL,
                                                config->prioridad, &s_tarea,
                                                config->nucleo) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "no hay memoria para la tarea");

    ESP_LOGI(TAG, "tarea periodica de %" PRIu32 " ms, prioridad %u, nucleo %d: un resumen "
                  "cada %" PRIu32 " intervalos",
             config->periodo_ms, (unsigned)config->prioridad, (int)config->nucleo,
             config->intervalos);
    return ESP_OK;
}

bool sensor_ultima_medida(bme280_medida_t *m)
{
    portENTER_CRITICAL(&s_cerrojo);
    const bool hay = s_hay_medida;
    *m = s_medida;
    portEXIT_CRITICAL(&s_cerrojo);
    return hay;
}

bool sensor_ultimo_resumen(carga_modo_t modo, resumen_datos_t *d, UBaseType_t *prioridad)
{
    if ((unsigned)modo >= CARGA_N_MODOS)
    {
        return false;
    }
    portENTER_CRITICAL(&s_cerrojo);
    const bool hay = s_hay_resumen[modo];
    *d = s_resumen[modo];
    *prioridad = s_prioridad_resumen[modo];
    portEXIT_CRITICAL(&s_cerrojo);
    return hay;
}

void sensor_progreso(uint32_t *llevados, uint32_t *total)
{
    portENTER_CRITICAL(&s_cerrojo);
    *llevados = s_llevados;
    portEXIT_CRITICAL(&s_cerrojo);
    *total = s_config.intervalos;
}

uint32_t sensor_periodo_ms(void)
{
    return s_config.periodo_ms;
}

esp_err_t sensor_poner_prioridad(UBaseType_t prioridad)
{
    ESP_RETURN_ON_FALSE(s_tarea, ESP_ERR_INVALID_STATE, TAG, "no iniciado");
    ESP_RETURN_ON_FALSE(prioridad > 0 && prioridad < configMAX_PRIORITIES,
                        ESP_ERR_INVALID_ARG, TAG, "prioridad fuera de rango");
    vTaskPrioritySet(s_tarea, prioridad);
    s_generacion_prioridad = s_generacion_prioridad + 1;
    return ESP_OK;
}

UBaseType_t sensor_prioridad(void)
{
    return s_tarea ? uxTaskPriorityGet(s_tarea) : 0;
}
