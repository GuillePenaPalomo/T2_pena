#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "consola.h"
#include "led.h"
#include "pulsador.h"
#include "bme280.h"
#include "bus_i2c.h"

#include "pines.h"

static const char *TAG = "Taller 2";

/* Valores de la configuración del proyecto, en microsegundos salvo que se
 * diga otra cosa. */
#define TRABAJO_REGISTRO_MS CONFIG_SEYTR_TRABAJO_REGISTRO_MS

/* Prioridades de la variante en tareas. La del pulsador es la más alta
 * porque es la actividad con el plazo más corto; el criterio se estudia en
 * U3-S5. La tarea inactiva tiene prioridad 0 y app_main, 1. */
#define PRIORIDAD_CARGA     6
#define PRIORIDAD_INFORME   6
#define NUCLEO0             0
#define NUCLEO1             1


/* ------------------------------------------------------------------------
 * Las medidas.
 * ------------------------------------------------------------------------ */

/* Escribe una duración en microsegundos como milisegundos con una cifra
 * decimal y coma decimal, por ejemplo 12,3. */
static void escribir_ms(char *destino, size_t tam, int64_t us)
{
    // destino: donde lo quieres imprimir
    // tam: el tamaño de lo que quieres imprimir
    if (us < 0) us = 0;
    snprintf(destino, tam, "%" PRId64 ",%" PRId64, us / 1000, (us % 1000) / 100);
    // PRId64 significa imprime un entero sin signo de 64 bits.
}

static uint32_t s_informes = 0;

static void escribir_informe(void *argumento)
{
    (void)argumento;
    s_informes++;
    printf("registro: informe %" PRIu32 " escrito\n", s_informes);
}

/* ------------------------------------------------------------------------
 * Tareas.
 * ------------------------------------------------------------------------ */

static void trabajar_ms(uint32_t ms) {
    const int64_t fin = esp_timer_get_time() + (int64_t)ms * 1000;
    uint64_t cuenta = 0;
    
    while (esp_timer_get_time() < fin) {
        cuenta++;
    }

}

static void carga_1(void *argumento)
{
    (void)argumento;
    for (;;) {
        trabajar_ms(TRABAJO_REGISTRO_MS);
        vTaskDelay(pdMS_TO_TICKS(27));
    }
}

static void carga_2(void *argumento) 
{
    (void)argumento;
    (void)argumento;
    for (;;) {
        trabajar_ms(TRABAJO_REGISTRO_MS);
        vTaskDelay(pdMS_TO_TICKS(27));
    }
}


static void crear_tareas(void)
{
    bool ok = true;

    ok = ok && xTaskCreatePinnedToCore(carga_1, "carga", 3072, NULL,
                                       PRIORIDAD_CARGA, NULL, NUCLEO0) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(carga_2, "carga", 3072, NULL,
                                       PRIORIDAD_CARGA, NULL, NUCLEO0) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(escribir_informe, "informe", 3072, NULL,
                                       PRIORIDAD_INFORME, NULL, NUCLEO1) == pdPASS;
    
    if (!ok) {
        ESP_LOGE(TAG, "no hay memoria para las tareas");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "SEyTR - Taller 2. Tick a %d Hz", configTICK_RATE_HZ);
    if (configTICK_RATE_HZ != 1000) {
        ESP_LOGW(TAG, "el tick no esta a 1000 Hz, que es el valor del curso");
    }

    crear_tareas();

}