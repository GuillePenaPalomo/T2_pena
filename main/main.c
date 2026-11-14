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

#include "bme280.h"
#include "bus_i2c.h"
#include "carga.h"
#include "comandos.h"
#include "inspeccion.h"
#include "led.h"
#include "pulsador.h"
#include "sensor.h"
#include "temporizadores.h"

#include "pines.h"
#include "tareas.h"

static const char *TAG = "taller2";

/* Dirección del BME280 de la placa del montaje, con SDO a masa, y velocidad
 * del bus. No son pines: no van en pines.h. */
#define DIRECCION_BME280 0x76
#define VELOCIDAD_I2C_HZ 100000

/* La espera tiene que ser mayor que la ráfaga: si no, con las dos tareas de
 * carga desfasadas podría haber siempre una lista, y la tarea inactiva del
 * núcleo 0 no llegaría a ejecutarse tampoco en turnos. */
_Static_assert(CONFIG_SEYTR_CARGA_ESPERA_MS > CONFIG_SEYTR_CARGA_RAFAGA_MS,
               "la espera de la carga tiene que ser mayor que su rafaga");

/* Si el ciclo de la carga dividiera el periodo del sensor, cada activación
 * caería en el mismo punto del ciclo: un retraso constante que el jitter no
 * ve. */
_Static_assert(CONFIG_SEYTR_SENSOR_PERIODO_MS %
                       (CONFIG_SEYTR_CARGA_RAFAGA_MS + CONFIG_SEYTR_CARGA_ESPERA_MS) != 0,
               "el ciclo de la carga no debe ser divisor del periodo del sensor");

static bus_i2c_t s_bus;
static bme280_t s_bme280;
static pulsador_t s_pulsador;

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

static void iniciar_sensor(void)
{
    ESP_ERROR_CHECK(bus_i2c_iniciar(&s_bus, PIN_SDA, PIN_SCL, false));
    ESP_ERROR_CHECK(bme280_iniciar(&s_bme280, s_bus.manejador, DIRECCION_BME280,
                                   VELOCIDAD_I2C_HZ));
    const esp_err_t err = bme280_configurar(&s_bme280);
    if (err != ESP_OK)
    {
        /* Sin sensor no hay tarea periódica, pero el resto sigue. */
        ESP_LOGE(TAG, "el BME280 no responde en 0x%02X (%s): se explora el bus",
                 DIRECCION_BME280, esp_err_to_name(err));
        bus_i2c_exploracion_t r;
        bus_i2c_explorar(&s_bus, &r);
        for (int i = 0; i < r.n_encontradas; i++)
        {
            ESP_LOGW(TAG, "responde un dispositivo en 0x%02X", r.encontradas[i]);
        }
        ESP_LOGW(TAG, "exploradas %d direcciones: %d sin respuesta, %d tiempo agotado%s",
                 r.exploradas, r.n_sin_respuesta, r.n_tiempo_agotado,
                 r.interrumpida ? " (interrumpida: el bus no funciona)" : "");
        return;
    }

    const sensor_config_t config = {
        .bme280 = &s_bme280,
        .periodo_ms = CONFIG_SEYTR_SENSOR_PERIODO_MS,
        .intervalos = CONFIG_SEYTR_SENSOR_INTERVALOS,
        .prioridad = PRIORIDAD_SENSOR,
        .nucleo = NUCLEO_PRUEBA,
        .pila = PILA_SENSOR,
    };
    ESP_ERROR_CHECK(sensor_iniciar(&config));
}

/* Lo que se hace con cada pulsación válida: pedir al LED el modo siguiente.
 * Se ejecuta en la tarea del pulsador; solo deja la orden en la cola del LED. */
static void al_pulsar(uint32_t pulsaciones, void *argumento)
{
    (void)pulsaciones;
    (void)argumento;
    if (led_siguiente_modo() != ESP_OK)
    {
        ESP_LOGW(TAG, "la cola del LED esta llena: se pierde la pulsacion");
    }
}

static void iniciar_led_y_pulsador(void)
{
    const led_config_t led = {
        .pin = PIN_LED,
        .nivel_encendido = LED_NIVEL_ENCENDIDO,
        .frecuencia_hz = CONFIG_SEYTR_LED_FRECUENCIA_HZ,
        .resolucion_bits = CONFIG_SEYTR_LED_RESOLUCION_BITS,
        .medio_porcentaje = CONFIG_SEYTR_LED_MEDIO_PORCENTAJE,
        .transicion_ms = CONFIG_SEYTR_LED_TRANSICION_MS,
        .prioridad = PRIORIDAD_LED,
        .nucleo = NUCLEO_PRUEBA,
        .pila = PILA_LED,
    };
    ESP_ERROR_CHECK(led_iniciar(&led));

    ESP_ERROR_CHECK(pulsador_iniciar(&s_pulsador, PIN_PULSADOR, PULSADOR_NIVEL_PULSADO));
    /* El nivel de reposo lo fija la resistencia externa de la protoboard. La
     * interna se deja también activada, en paralelo: no cambia el nivel en
     * reposo, y si la externa hace mal contacto el pin no queda flotante. Sin
     * ella, la primera captura de P3 contó 19 pulsaciones de 8 y unos 20 000
     * flancos de ruido (resultados/p3_led_modos_pin_flotante.txt). */
    ESP_ERROR_CHECK(pulsador_polarizar(&s_pulsador, true));
    const pulsador_atencion_t atencion = {
        .guarda_ms = CONFIG_SEYTR_GUARDA_MS,
        .confirmacion_ms = CONFIG_SEYTR_CONFIRMACION_MS,
        .prioridad = PRIORIDAD_PULSADOR,
        .nucleo = NUCLEO_PRUEBA,
        .pila = PILA_PULSADOR,
        .aviso = al_pulsar,
        .argumento = NULL,
    };
    ESP_ERROR_CHECK(pulsador_atender(&s_pulsador, &atencion));
}

/* Unos vencimientos tienen que caer dentro de una ráfaga de la carga y otros
 * no: el ciclo de la carga no puede ser múltiplo del periodo. */
_Static_assert((CONFIG_SEYTR_CARGA_RAFAGA_MS + CONFIG_SEYTR_CARGA_ESPERA_MS) %
                       CONFIG_SEYTR_TEMPORIZADORES_PERIODO_MS != 0,
               "el ciclo de la carga no debe ser multiplo del periodo de los temporizadores");

static void iniciar_temporizadores(void)
{
    const temporizadores_config_t config = {
        .periodo_ms = CONFIG_SEYTR_TEMPORIZADORES_PERIODO_MS,
        .intervalos = CONFIG_SEYTR_SENSOR_INTERVALOS, /* el mismo número que P2 */
        .prioridad_medida = PRIORIDAD_MEDIDA,
        .nucleo_medida = NUCLEO_OBSERVACION,
        .pila_medida = PILA_MEDIDA,
    };
    ESP_ERROR_CHECK(temporizadores_iniciar(&config));
}

/* Las tareas propias, para la tabla de pilas del comando tareas. */
static const inspeccion_tarea_t s_tareas_propias[] = {
    {"carga_a", PILA_CARGA},
    {"carga_b", PILA_CARGA},
    {"informe", PILA_INFORME},
    {"sensor", PILA_SENSOR},
    {"led", PILA_LED},
    {"pulsador", PILA_PULSADOR},
    {"medida", PILA_MEDIDA},
    {"consola", PILA_CONSOLA},
};

void app_main(void)
{
    ESP_LOGI(TAG, "SEyTR - Taller 2. Tick a %d Hz", configTICK_RATE_HZ);
    if (configTICK_RATE_HZ != 1000)
    {
        ESP_LOGW(TAG, "el tick no esta a 1000 Hz, que es el valor del curso");
    }

    iniciar_carga();
    iniciar_sensor();
    iniciar_led_y_pulsador();
    iniciar_temporizadores();
    ESP_ERROR_CHECK(inspeccion_iniciar(s_tareas_propias,
                                       sizeof s_tareas_propias / sizeof s_tareas_propias[0]));

    /* La consola, al final: sus comandos llaman a los demás componentes. */
    ESP_ERROR_CHECK(comandos_iniciar());

    /* app_main termina aquí; las tareas siguen. */
}
