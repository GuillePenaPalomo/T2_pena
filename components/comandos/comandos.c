/* comandos.c - los comandos de la consola del Taller 2. Ver comandos.h. */
#include "comandos.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

#include "carga.h"
#include "consola.h"
#include "led.h"
#include "pulsador.h"
#include "sensor.h"

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

/* ---- P2: la tarea periódica del sensor ------------------------------------- */

static void comando_sensor(const char *argumentos)
{
    unsigned prioridad;
    char extra;
    if (sscanf(argumentos, "prio %u %c", &prioridad, &extra) == 1)
    {
        const esp_err_t err = sensor_poner_prioridad((UBaseType_t)prioridad);
        if (err != ESP_OK)
        {
            printf("No se pudo cambiar la prioridad: %s\n", esp_err_to_name(err));
            return;
        }
        printf("Prioridad del sensor: %u. La medida empieza de nuevo.\n", prioridad);
        return;
    }
    if (*argumentos != '\0')
    {
        printf("Uso: sensor [prio <n>]\n");
        return;
    }

    bme280_medida_t m;
    if (sensor_ultima_medida(&m))
    {
        const int32_t t = m.temperatura_centigrados;
        const uint32_t ta = (uint32_t)(t < 0 ? -t : t);
        printf("Ultima medida: T=%s%" PRIu32 ",%02" PRIu32 " C, P=%" PRIu32 ",%02" PRIu32
               " hPa, HR=%" PRIu32 ",%02" PRIu32 " %%\n",
               t < 0 ? "-" : "", ta / 100, ta % 100,
               m.presion_pa / 100, m.presion_pa % 100,
               m.humedad_centiporcentaje / 100, m.humedad_centiporcentaje % 100);
    }
    else
    {
        printf("Todavia no hay ninguna medida del sensor\n");
    }

    uint32_t llevados, total;
    sensor_progreso(&llevados, &total);
    printf("Tarea sensor: periodo %" PRIu32 " ms, prioridad %u. Resumen en curso: %" PRIu32
           " de %" PRIu32 " intervalos, con la carga en %s\n",
           sensor_periodo_ms(), (unsigned)sensor_prioridad(), llevados, total,
           carga_nombre_modo(carga_modo()));

    for (int i = 0; i < CARGA_N_MODOS; i++)
    {
        resumen_datos_t d;
        UBaseType_t prio_resumen;
        if (sensor_ultimo_resumen((carga_modo_t)i, &d, &prio_resumen))
        {
            char etiqueta[64];
            snprintf(etiqueta, sizeof etiqueta, "  carga %s, prioridad %u",
                     carga_nombre_modo((carga_modo_t)i), (unsigned)prio_resumen);
            resumen_escribir(etiqueta, &d, (int64_t)sensor_periodo_ms() * 1000);
        }
        else
        {
            printf("  carga %s: sin resumen completo\n", carga_nombre_modo((carga_modo_t)i));
        }
    }
}

/* ---- P3: el LED con LEDC --------------------------------------------------- */

/* La consola no llama a LEDC: manda la orden a la tarea del LED. Si llamara
 * ella durante una transición, ledc_set_duty_and_update() la dejaría
 * esperando hasta el final de la transición. */
static void comando_led(const char *argumentos)
{
    esp_err_t err = ESP_OK;
    led_modo_t modo;
    unsigned porcentaje;
    char extra;

    if (*argumentos == '\0')
    {
        uint32_t ciclo, maximo, pulsaciones, descartados;
        led_estado(&modo, &ciclo, &maximo);
        pulsador_cuentas(&pulsaciones, &descartados);
        printf("LED: modo %s, ciclo de trabajo %" PRIu32 " de %" PRIu32
               ". Pulsaciones %" PRIu32 ", flancos descartados %" PRIu32 "\n",
               led_nombre_modo(modo), ciclo, maximo, pulsaciones, descartados);
        return;
    }
    if (strcmp(argumentos, "siguiente") == 0)
    {
        err = led_siguiente_modo();
    }
    else if (led_modo_por_nombre(argumentos, &modo))
    {
        err = led_poner_modo(modo);
    }
    else if (sscanf(argumentos, "%u %c", &porcentaje, &extra) == 1 && porcentaje <= 100)
    {
        err = led_poner_ciclo(porcentaje);
    }
    else
    {
        printf("Uso: led [siguiente|apagado|medio|encendido|transicion|0-100]\n");
        return;
    }
    if (err != ESP_OK)
    {
        printf("No se pudo mandar la orden al LED: %s\n", esp_err_to_name(err));
    }
}

/* ---- La tabla -------------------------------------------------------------- */

static const consola_comando_t s_comandos[] = {
    {"help", NULL, "Muestra esta lista de comandos", comando_ayuda},
    {"carga", "parada|turnos|inanicion|corregida", "Cambia el modo de la carga; sin argumento, lo muestra",
     comando_carga},
    {"informe", "on|off", "Muestra u oculta el informe de la carga de cada segundo",
     comando_informe},
    {"sensor", "[prio <n>]", "Ultima medida y ultimo resumen de cada modo; prio cambia la prioridad",
     comando_sensor},
    {"led", "[siguiente|apagado|medio|encendido|transicion|0-100]",
     "Modo del LED o ciclo de trabajo en %; sin argumento, el estado", comando_led},
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
