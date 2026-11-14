/* carga.c - dos tareas de cálculo que cargan un núcleo, y su informe. Ver carga.h. */
#include "carga.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "carga";

#define PERIODO_INFORME_MS 1000

/* Lo que tiene cada tarea de carga. La cuenta la escribe solo su tarea y la
 * lee la del informe: es de 32 bits, que en este procesador se leen y se
 * escriben de una vez. */
typedef struct
{
    const char *nombre;
    bool acapara;             /* la que deja de bloquearse en inanición */
    volatile uint32_t cuenta; /* vueltas de cálculo desde el arranque */
    TaskHandle_t tarea;
} carga_tarea_t;

static carga_tarea_t s_a = {.nombre = "carga_a", .acapara = true};
static carga_tarea_t s_b = {.nombre = "carga_b", .acapara = false};

static carga_config_t s_config;
static volatile carga_modo_t s_modo = CARGA_PARADA;
static volatile uint32_t s_generacion = 0;
static volatile bool s_informar = true;

static const char *const NOMBRES[CARGA_N_MODOS] = {
    [CARGA_PARADA] = "parada",
    [CARGA_TURNOS] = "turnos",
    [CARGA_INANICION] = "inanicion",
    [CARGA_CORREGIDA] = "corregida",
};

/* La ráfaga de cálculo. Es una espera activa: ocupa el procesador hasta que
 * han pasado «ms» milisegundos de reloj, y cuenta las vueltas que da. Si otra
 * tarea le quita el procesador a mitad, da menos vueltas: por eso la cuenta
 * dice cuánto procesador ha recibido la tarea. */
static void calcular(carga_tarea_t *c, uint32_t ms)
{
    const int64_t fin = esp_timer_get_time() + (int64_t)ms * 1000;
    while (esp_timer_get_time() < fin)
    {
        c->cuenta = c->cuenta + 1;
    }
}

static void tarea_carga(void *argumento)
{
    carga_tarea_t *c = argumento;

    for (;;)
    {
        const carga_modo_t modo = s_modo;
        if (modo == CARGA_PARADA)
        {
            /* Bloqueada sin plazo hasta que carga_poner_modo() avise. */
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }

        calcular(c, s_config.rafaga_ms);

        if (modo == CARGA_INANICION && c->acapara)
        {
            /* Sin espera: la tarea no se bloquea nunca. Con más prioridad que
             * la otra tarea de carga y que la inactiva, ninguna de las dos
             * vuelve a ejecutarse en este núcleo. */
            continue;
        }
        /* Bloqueada: aquí se ejecutan las tareas de menos prioridad de su
         * núcleo, entre ellas la inactiva. */
        vTaskDelay(pdMS_TO_TICKS(s_config.espera_ms));
    }
}

static void tarea_informe(void *argumento)
{
    (void)argumento;
    uint32_t vista_a = 0;
    uint32_t vista_b = 0;
    TickType_t referencia = xTaskGetTickCount();

    for (;;)
    {
        xTaskDelayUntil(&referencia, pdMS_TO_TICKS(PERIODO_INFORME_MS));

        const uint32_t a = s_a.cuenta;
        const uint32_t b = s_b.cuenta;
        /* Aritmética sin signo: la resta admite que la cuenta dé la vuelta. */
        const uint32_t avance_a = a - vista_a;
        const uint32_t avance_b = b - vista_b;
        vista_a = a;
        vista_b = b;

        const carga_modo_t modo = s_modo;
        if (!s_informar || modo == CARGA_PARADA)
        {
            continue;
        }
        /* El instante va en milisegundos desde el arranque, la misma escala
         * que la marca de los avisos del watchdog: E (...) task_wdt. */
        printf("carga %s (t = %" PRIu32 " ms): %s prio %u avanza %" PRIu32
               "; %s prio %u avanza %" PRIu32 "%s\n",
               NOMBRES[modo], esp_log_timestamp(),
               s_a.nombre, (unsigned)uxTaskPriorityGet(s_a.tarea), avance_a,
               s_b.nombre, (unsigned)uxTaskPriorityGet(s_b.tarea), avance_b,
               avance_b == 0 ? " (PARADA: no recibe procesador)" : "");
    }
}

esp_err_t carga_iniciar(const carga_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "puntero nulo");
    ESP_RETURN_ON_FALSE(config->rafaga_ms > 0 && config->espera_ms > config->rafaga_ms,
                        ESP_ERR_INVALID_ARG, TAG,
                        "la espera tiene que ser mayor que la rafaga");
    ESP_RETURN_ON_FALSE(config->prioridad_alta > config->prioridad,
                        ESP_ERR_INVALID_ARG, TAG,
                        "prioridad_alta tiene que ser mayor que prioridad");
    ESP_RETURN_ON_FALSE(s_a.tarea == NULL, ESP_ERR_INVALID_STATE, TAG,
                        "la carga ya esta iniciada");
    s_config = *config;

    bool ok = xTaskCreatePinnedToCore(tarea_carga, s_a.nombre, config->pila, &s_a,
                                      config->prioridad, &s_a.tarea,
                                      config->nucleo) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(tarea_carga, s_b.nombre, config->pila, &s_b,
                                       config->prioridad, &s_b.tarea,
                                       config->nucleo) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(tarea_informe, "informe", config->pila_informe,
                                       NULL, config->prioridad_informe, NULL,
                                       config->nucleo_informe) == pdPASS;
    ESP_RETURN_ON_FALSE(ok, ESP_ERR_NO_MEM, TAG, "no hay memoria para las tareas");

    ESP_LOGI(TAG, "dos tareas de carga en el nucleo %d, prioridad %u: %" PRIu32
                  " ms de calculo y %" PRIu32 " ms de espera. Empieza parada",
             (int)config->nucleo, (unsigned)config->prioridad,
             config->rafaga_ms, config->espera_ms);
    return ESP_OK;
}

esp_err_t carga_poner_modo(carga_modo_t modo)
{
    ESP_RETURN_ON_FALSE((unsigned)modo < CARGA_N_MODOS, ESP_ERR_INVALID_ARG, TAG,
                        "modo no valido");
    ESP_RETURN_ON_FALSE(s_a.tarea && s_b.tarea, ESP_ERR_INVALID_STATE, TAG,
                        "la carga no esta iniciada");

    const bool alta = (modo == CARGA_INANICION || modo == CARGA_CORREGIDA);
    s_modo = modo;
    s_generacion = s_generacion + 1;
    vTaskPrioritySet(s_a.tarea, alta ? s_config.prioridad_alta : s_config.prioridad);

    /* Si estaban paradas, las despierta; si no, el aviso queda anotado y lo
     * descarta la siguiente vez que se paren. */
    xTaskNotifyGive(s_a.tarea);
    xTaskNotifyGive(s_b.tarea);
    return ESP_OK;
}

carga_modo_t carga_modo(void)
{
    return s_modo;
}

uint32_t carga_generacion(void)
{
    return s_generacion;
}

uint32_t carga_ciclo_ms(void)
{
    return s_config.rafaga_ms + s_config.espera_ms;
}

const char *carga_nombre_modo(carga_modo_t modo)
{
    return (unsigned)modo < CARGA_N_MODOS ? NOMBRES[modo] : "?";
}

bool carga_modo_por_nombre(const char *nombre, carga_modo_t *modo)
{
    for (int i = 0; i < CARGA_N_MODOS; i++)
    {
        if (strcmp(nombre, NOMBRES[i]) == 0)
        {
            *modo = (carga_modo_t)i;
            return true;
        }
    }
    return false;
}

void carga_informar(bool activar)
{
    s_informar = activar;
}

bool carga_informa(void)
{
    return s_informar;
}
