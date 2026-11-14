/* main.c - SEyTR, Taller 2: tareas, prioridades y temporización.
 *
 * Este fichero solo inicia y arranca. Cada módulo está en su componente, en
 * components/, y no sabe ni en qué pin está su periférico ni con qué prioridad
 * corre su tarea: se lo dice app_main, que lo toma de dos ficheros.
 *   pines.h   los pines del proyecto
 *   tareas.h  la prioridad, el núcleo y la pila de cada tarea
 *
 * Qué hardware necesita
 * ---------------------
 * La placa del curso, conectada por su conector USB-a-UART, que es por donde
 * va la consola. El montaje del Taller 1: LED, pulsador y sensor BME280.
 */

#include "freertos/FreeRTOS.h"

#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "carga.h"
#include "comandos.h"

#include "tareas.h"

static const char *TAG = "taller2";

/* La espera tiene que ser mayor que la ráfaga: si no, con las dos tareas de
 * carga desfasadas podría haber siempre una lista, y la tarea inactiva del
 * núcleo 0 no llegaría a ejecutarse tampoco en turnos. */
_Static_assert(CONFIG_SEYTR_CARGA_ESPERA_MS > CONFIG_SEYTR_CARGA_RAFAGA_MS,
               "la espera de la carga tiene que ser mayor que su rafaga");

static void iniciar_carga(void)
{
    const carga_config_t config = {
        .rafaga_ms = CONFIG_SEYTR_CARGA_RAFAGA_MS,
        .espera_ms = CONFIG_SEYTR_CARGA_ESPERA_MS,
        .prioridad = PRIORIDAD_CARGA,
        .prioridad_alta = PRIORIDAD_CARGA_ALTA,
        .nucleo = NUCLEO_PRUEBA,
        .pila = PILA_CARGA,
        .prioridad_informe = PRIORIDAD_INFORME,
        .nucleo_informe = NUCLEO_OBSERVACION,
        .pila_informe = PILA_INFORME,
    };
    ESP_ERROR_CHECK(carga_iniciar(&config));
}

void app_main(void)
{
    ESP_LOGI(TAG, "SEyTR - Taller 2. Tick a %d Hz", configTICK_RATE_HZ);
    if (configTICK_RATE_HZ != 1000)
    {
        ESP_LOGW(TAG, "el tick no esta a 1000 Hz, que es el valor del curso");
    }

    iniciar_carga();

    /* La consola, al final: sus comandos llaman a los demás componentes. */
    ESP_ERROR_CHECK(comandos_iniciar());

    /* app_main termina aquí; las tareas siguen. */
}
