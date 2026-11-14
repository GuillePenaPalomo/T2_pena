/* comandos.c - los comandos de la consola del Taller 2. Ver comandos.h. */
#include "comandos.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

#include "carga.h"
#include "consola.h"

static const char *TAG = "comandos";

#define LARGO_MAXIMO_LINEA 64

static void comando_ayuda(const char *argumentos)
{
    (void)argumentos;
    consola_ayuda();
}

/* ---- P1: la carga ---------------------------------------------------------- */

static void comando_carga(const char *argumentos)
{
    if (*argumentos == '\0')
    {
        printf("Carga: %s. Uso: carga <parada|turnos|inanicion|corregida>\n",
               carga_nombre_modo(carga_modo()));
        return;
    }

    carga_modo_t modo;
    if (!carga_modo_por_nombre(argumentos, &modo))
    {
        printf("No conozco el modo \"%s\". Uso: carga <parada|turnos|inanicion|corregida>\n",
               argumentos);
        return;
    }

    const esp_err_t err = carga_poner_modo(modo);
    if (err != ESP_OK)
    {
        printf("No se pudo cambiar el modo de la carga: %s\n", esp_err_to_name(err));
        return;
    }

    printf("Carga: %s (t = %lu ms)\n", carga_nombre_modo(modo),
           (unsigned long)esp_log_timestamp());
    if (modo == CARGA_INANICION)
    {
        printf("carga_a no se bloquea: el aviso del watchdog deberia salir en unos %d s\n",
               CONFIG_ESP_TASK_WDT_TIMEOUT_S);
    }
}

static void comando_informe(const char *argumentos)
{
    if (strcmp(argumentos, "on") == 0)
    {
        carga_informar(true);
    }
    else if (strcmp(argumentos, "off") == 0)
    {
        carga_informar(false);
    }
    else
    {
        printf("Uso: informe <on|off>\n");
        return;
    }
    printf("Informe de la carga: %s\n", carga_informa() ? "visible" : "oculto");
}

/* ---- La tabla -------------------------------------------------------------- */

static const consola_comando_t s_comandos[] = {
    {"help", NULL, "Muestra esta lista de comandos", comando_ayuda},
    {"carga", "parada|turnos|inanicion|corregida", "Cambia el modo de la carga; sin argumento, lo muestra",
     comando_carga},
    {"informe", "on|off", "Muestra u oculta el informe de la carga de cada segundo",
     comando_informe},
};

esp_err_t comandos_iniciar(void)
{
    const consola_config_t configuracion = {
        .puerto = CONFIG_ESP_CONSOLE_UART_NUM, /* la UART del monitor */
        .largo_maximo = LARGO_MAXIMO_LINEA,
        .tabla = s_comandos,
        .n_comandos = sizeof(s_comandos) / sizeof(s_comandos[0]),
    };

    ESP_LOGI(TAG, "consola en la UART%d: escribe help para ver los comandos",
             CONFIG_ESP_CONSOLE_UART_NUM);
    return consola_iniciar(&configuracion);
}
