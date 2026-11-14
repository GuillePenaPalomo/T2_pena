/* inspeccion.c - lista de tareas, reparto del procesador y pila. Ver inspeccion.h.
 * Sigue informe_completo() y carga.c del ejemplo de la asignatura
 * SEyTR_depuracion_de_tareas. */
#include "inspeccion.h"

#include <inttypes.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "inspeccion";

#define MAX_TAREAS 32

/* Unos 40 bytes por tarea, según la documentación de vTaskList() en task.h.
 * El programa tiene una veintena: 2048 sobran. */
static char s_tabla[2048];
static TaskStatus_t s_estado[MAX_TAREAS];

static const inspeccion_tarea_t *s_propias;
static uint32_t s_n_propias;

/* Lo de la llamada anterior, para la carga del intervalo. */
static bool s_hay_anterior;
static configRUN_TIME_COUNTER_TYPE s_inactiva_antes[2];
static configRUN_TIME_COUNTER_TYPE s_total_antes;

esp_err_t inspeccion_iniciar(const inspeccion_tarea_t *tabla, uint32_t n)
{
    ESP_RETURN_ON_FALSE(tabla && n > 0, ESP_ERR_INVALID_ARG, TAG, "tabla vacia");
    s_propias = tabla;
    s_n_propias = n;
    return ESP_OK;
}

/* Tiempo acumulado de la tarea inactiva de cada núcleo, y el total, con
 * uxTaskGetSystemState(), lo que usa por dentro vTaskGetRunTimeStats(). */
static void leer_inactivas(configRUN_TIME_COUNTER_TYPE inactiva[2],
                           configRUN_TIME_COUNTER_TYPE *total)
{
    const TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCore(0);
    const TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCore(1);
    inactiva[0] = inactiva[1] = 0;
    *total = 0;
    const UBaseType_t n = uxTaskGetSystemState(s_estado, MAX_TAREAS, total);
    for (UBaseType_t i = 0; i < n; i++)
    {
        if (s_estado[i].xHandle == idle0)
        {
            inactiva[0] = s_estado[i].ulRunTimeCounter;
        }
        else if (s_estado[i].xHandle == idle1)
        {
            inactiva[1] = s_estado[i].ulRunTimeCounter;
        }
    }
}

static void escribir_carga_del_intervalo(void)
{
    configRUN_TIME_COUNTER_TYPE inactiva[2];
    configRUN_TIME_COUNTER_TYPE total;
    leer_inactivas(inactiva, &total);

    if (s_hay_anterior)
    {
        /* El total es el tiempo transcurrido, en microsegundos, con el reloj
         * de esp_timer. Las restas sin signo admiten que el contador de 32
         * bits dé la vuelta una vez: unos 71 minutos. */
        const uint32_t dt = (uint32_t)(total - s_total_antes);
        printf("\n--- Carga de cada nucleo en los ultimos %" PRIu32 " ms (100 %% menos la tarea inactiva) ---\n",
               dt / 1000);
        for (int c = 0; c < 2 && dt > 0; c++)
        {
            const uint32_t libre = (uint32_t)((uint64_t)(uint32_t)(inactiva[c] - s_inactiva_antes[c]) *
                                              1000u / dt);
            printf("  nucleo %d: inactiva %" PRIu32 ",%" PRIu32 " %%, carga %" PRIu32 ",%" PRIu32 " %%\n",
                   c, libre / 10, libre % 10, (1000 - libre) / 10, (1000 - libre) % 10);
        }
    }
    else
    {
        printf("\n(Primera captura: la carga del intervalo sale en la siguiente.)\n");
    }
    s_hay_anterior = true;
    s_inactiva_antes[0] = inactiva[0];
    s_inactiva_antes[1] = inactiva[1];
    s_total_antes = total;
}

static void escribir_pilas(void)
{
    printf("\n--- Pila de cada tarea propia (uxTaskGetStackHighWaterMark) ---\n");
    printf("  %-10s %5s %6s %10s %10s\n", "Tarea", "Prio", "Nucleo", "Asignada", "Min libre");
    for (uint32_t i = 0; i < s_n_propias; i++)
    {
        const TaskHandle_t t = xTaskGetHandle(s_propias[i].nombre);
        if (t == NULL)
        {
            printf("  %-10s no existe\n", s_propias[i].nombre);
            continue;
        }
        const BaseType_t nucleo = xTaskGetCoreID(t);
        char texto_nucleo[12];
        if (nucleo == tskNO_AFFINITY)
        {
            snprintf(texto_nucleo, sizeof texto_nucleo, "-");
        }
        else
        {
            snprintf(texto_nucleo, sizeof texto_nucleo, "%d", (int)nucleo);
        }
        printf("  %-10s %5u %6s %10" PRIu32 " %10u\n", s_propias[i].nombre,
               (unsigned)uxTaskPriorityGet(t), texto_nucleo, s_propias[i].pila,
               (unsigned)uxTaskGetStackHighWaterMark(t));
    }
}

void inspeccion_escribir(void)
{
    printf("\n--- Lista de tareas (vTaskList) ---\n");
    printf("Nombre\t\tEstado\tPrio\tPila\tNum\tNucleo\n");
    vTaskList(s_tabla);
    printf("%s", s_tabla);
    printf("Estado: X en ejecucion (la que pide la lista), R lista, B bloqueada,\n"
           "S suspendida o bloqueada sin plazo, D borrada. Pila: minimo libre, en bytes.\n"
           "Nucleo: -1 sin afinidad.\n");

    printf("\n--- Reparto del procesador desde el arranque (vTaskGetRunTimeStats) ---\n");
    printf("Nombre\t\tTiempo (us)\tPorcentaje\n");
    vTaskGetRunTimeStats(s_tabla);
    printf("%s", s_tabla);
    printf("Con dos nucleos, los porcentajes suman hasta 200 %%.\n");

    escribir_carga_del_intervalo();
    escribir_pilas();

    printf("\nMemoria libre: %u bytes; minima desde el arranque: %u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
}
