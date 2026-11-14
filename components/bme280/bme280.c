/* bme280.c - el sensor BME280 como dispositivo del bus I2C. Ver bme280.h.
 *
 * La calibración, la configuración y la compensación proceden del Taller 1
 * (main/sensor_bme280.c de aquel proyecto), trasladadas a este componente
 * para que la tarea del sensor solo tenga que pedir una medida.
 */
#include "bme280.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "bme280";

/* Espera máxima de una transacción, en milisegundos. */
#define ESPERA_MS 100

/* Espera máxima de una conversión, en microsegundos. */
#define ESPERA_CONVERSION_US 100000

/* Registros de la hoja de datos. */
#define REG_CALIB_TP 0x88 /* T1..P9: 24 bytes */
#define REG_CALIB_H1 0xA1
#define REG_CALIB_H2 0xE1 /* H2..H6: 7 bytes */
#define REG_CTRL_HUM 0xF2
#define REG_STATUS 0xF3
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG 0xF5
#define REG_DATOS 0xF7 /* presión, temperatura y humedad: 8 bytes */

/* ctrl_meas: sobremuestreo x1 de temperatura (bits 7-5) y de presión (bits
 * 4-2), y el modo en los bits 1-0: 00 reposo, 01 forzado. */
#define CTRL_MEAS_REPOSO 0x24
#define CTRL_MEAS_FORZADO 0x25
#define STATUS_MIDIENDO 0x08

esp_err_t bme280_iniciar(bme280_t *s, i2c_master_bus_handle_t bus,
                         uint8_t direccion, uint32_t velocidad_hz)
{
    ESP_RETURN_ON_FALSE(s && bus, ESP_ERR_INVALID_ARG, TAG, "puntero nulo");
    ESP_RETURN_ON_FALSE(s->manejador == NULL, ESP_ERR_INVALID_STATE, TAG,
                        "el sensor ya esta colgado: quitalo antes");
    s->direccion = direccion;
    s->velocidad_hz = velocidad_hz;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = direccion,
        .scl_speed_hz = velocidad_hz,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s->manejador);
    if (err != ESP_OK)
    {
        s->manejador = NULL;
    }
    return err;
}

esp_err_t bme280_quitar(bme280_t *s)
{
    ESP_RETURN_ON_FALSE(s, ESP_ERR_INVALID_ARG, TAG, "puntero nulo");
    if (s->manejador == NULL)
    {
        return ESP_OK;
    }
    esp_err_t err = i2c_master_bus_rm_device(s->manejador);
    if (err == ESP_OK)
    {
        s->manejador = NULL;
    }
    return err;
}

/* ---- Acceso a los registros ------------------------------------------------ */

static esp_err_t leer_registros(bme280_t *s, uint8_t registro, uint8_t *datos, size_t n)
{
    return i2c_master_transmit_receive(s->manejador, &registro, 1, datos, n, ESPERA_MS);
}

static esp_err_t escribir_registro(bme280_t *s, uint8_t registro, uint8_t valor)
{
    const uint8_t datos[] = {registro, valor};
    return i2c_master_transmit(s->manejador, datos, sizeof(datos), ESPERA_MS);
}

esp_err_t bme280_leer_id(bme280_t *s, uint8_t *id)
{
    ESP_RETURN_ON_FALSE(s && s->manejador && id, ESP_ERR_INVALID_STATE, TAG,
                        "el sensor no esta colgado del bus");
    *id = 0;
    return leer_registros(s, BME280_REG_ID, id, 1);
}

/* ---- Calibración y configuración ------------------------------------------- */

static uint16_t u16_le(const uint8_t *b)
{
    return (uint16_t)((uint16_t)b[1] << 8 | b[0]);
}

static int16_t i16_le(const uint8_t *b)
{
    return (int16_t)u16_le(b);
}

/* H4 y H5 son de 12 bits con signo, repartidos entre tres registros. */
static int16_t extender_signo_12(uint16_t valor)
{
    return (int16_t)((valor & 0x0800U) ? (int32_t)valor - 0x1000 : (int32_t)valor);
}

static esp_err_t leer_calibracion(bme280_t *s)
{
    uint8_t tp[24];
    uint8_t h1;
    uint8_t h[7];
    ESP_RETURN_ON_ERROR(leer_registros(s, REG_CALIB_TP, tp, sizeof tp), TAG, "calibracion T y P");
    ESP_RETURN_ON_ERROR(leer_registros(s, REG_CALIB_H1, &h1, 1), TAG, "calibracion H1");
    ESP_RETURN_ON_ERROR(leer_registros(s, REG_CALIB_H2, h, sizeof h), TAG, "calibracion H2-H6");

    bme280_calibracion_t *c = &s->calibracion;
    c->t1 = u16_le(&tp[0]);
    c->t2 = i16_le(&tp[2]);
    c->t3 = i16_le(&tp[4]);
    c->p1 = u16_le(&tp[6]);
    c->p2 = i16_le(&tp[8]);
    c->p3 = i16_le(&tp[10]);
    c->p4 = i16_le(&tp[12]);
    c->p5 = i16_le(&tp[14]);
    c->p6 = i16_le(&tp[16]);
    c->p7 = i16_le(&tp[18]);
    c->p8 = i16_le(&tp[20]);
    c->p9 = i16_le(&tp[22]);
    c->h1 = h1;
    c->h2 = i16_le(&h[0]);
    c->h3 = h[2];
    c->h4 = extender_signo_12((uint16_t)((uint16_t)h[3] << 4 | (h[4] & 0x0FU)));
    c->h5 = extender_signo_12((uint16_t)((uint16_t)h[5] << 4 | (h[4] >> 4)));
    c->h6 = (int8_t)h[6];

    /* P1 es divisor en la compensación de la presión: si vale 0, la
     * calibración no se ha leído bien. */
    ESP_RETURN_ON_FALSE(c->p1 != 0, ESP_ERR_INVALID_RESPONSE, TAG,
                        "calibracion no valida: dig_P1 es cero");
    return ESP_OK;
}

esp_err_t bme280_configurar(bme280_t *s)
{
    uint8_t id = 0;
    ESP_RETURN_ON_ERROR(bme280_leer_id(s, &id), TAG, "no se pudo leer la identificacion");
    ESP_RETURN_ON_FALSE(id == BME280_ID, ESP_ERR_INVALID_RESPONSE, TAG,
                        "identificacion 0x%02X, se esperaba 0x%02X", id, BME280_ID);
    ESP_RETURN_ON_ERROR(leer_calibracion(s), TAG, "no se pudo leer la calibracion");

    /* El orden de la hoja de datos: ctrl_hum solo surte efecto después de
     * escribir ctrl_meas (apartados 5.4.3 y 5.4.5). Primero se deja en
     * reposo, para poder escribir config. */
    ESP_RETURN_ON_ERROR(escribir_registro(s, REG_CTRL_MEAS, 0x00), TAG, "ctrl_meas");
    ESP_RETURN_ON_ERROR(escribir_registro(s, REG_CONFIG, 0x00), TAG, "config");
    ESP_RETURN_ON_ERROR(escribir_registro(s, REG_CTRL_HUM, 0x01), TAG, "ctrl_hum");
    ESP_RETURN_ON_ERROR(escribir_registro(s, REG_CTRL_MEAS, CTRL_MEAS_REPOSO), TAG, "ctrl_meas");

    ESP_LOGI(TAG, "BME280 en 0x%02X, identificacion 0x%02X, calibracion leida",
             s->direccion, id);
    return ESP_OK;
}

/* ---- Lectura y compensación ------------------------------------------------ */

static esp_err_t esperar_conversion(bme280_t *s)
{
    const int64_t inicio = esp_timer_get_time();
    for (;;)
    {
        uint8_t estado = 0;
        ESP_RETURN_ON_ERROR(leer_registros(s, REG_STATUS, &estado, 1), TAG, "status");
        if ((estado & STATUS_MIDIENDO) == 0)
        {
            return ESP_OK;
        }
        if (esp_timer_get_time() - inicio >= ESPERA_CONVERSION_US)
        {
            return ESP_ERR_TIMEOUT;
        }
        /* Bloqueada un tick mientras convierte: no ocupa el procesador. */
        vTaskDelay(1);
    }
}

static int32_t compensar_temperatura(const bme280_calibracion_t *c, int32_t adc, int64_t *t_fine)
{
    const int64_t var1 = ((((int64_t)adc >> 3) - ((int64_t)c->t1 << 1)) * c->t2) >> 11;
    const int64_t delta = ((int64_t)adc >> 4) - c->t1;
    const int64_t var2 = (((delta * delta) >> 12) * c->t3) >> 14;
    *t_fine = var1 + var2;
    return (int32_t)((*t_fine * 5 + 128) >> 8);
}

static uint32_t compensar_presion(const bme280_calibracion_t *c, int32_t adc, int64_t t_fine)
{
    int64_t var1 = t_fine - 128000;
    int64_t var2 = var1 * var1 * c->p6;
    var2 += (var1 * c->p5) * ((int64_t)1 << 17);
    var2 += (int64_t)c->p4 * ((int64_t)1 << 35);
    var1 = ((var1 * var1 * c->p3) >> 8) + ((var1 * c->p2) * ((int64_t)1 << 12));
    var1 = ((((int64_t)1 << 47) + var1) * c->p1) >> 33;
    if (var1 == 0)
    {
        return 0; /* evita dividir entre cero */
    }
    int64_t p = (((int64_t)1048576 - adc) * ((int64_t)1 << 31) - var2) * 3125 / var1;
    var1 = ((int64_t)c->p9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)c->p8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)c->p7 << 4);
    /* p está en Q24.8: pascales por 256. */
    return (uint32_t)((p + 128) >> 8);
}

static uint32_t compensar_humedad(const bme280_calibracion_t *c, int32_t adc, int64_t t_fine)
{
    int64_t h = t_fine - 76800;
    h = (((((int64_t)adc << 14) - ((int64_t)c->h4 << 20) - ((int64_t)c->h5 * h)) + 16384) >> 15) *
        (((((((h * c->h6) >> 10) * (((h * c->h3) >> 11) + 32768)) >> 10) + 2097152) * c->h2 +
          8192) >> 14);
    h -= (((((h >> 15) * (h >> 15)) >> 7) * c->h1) >> 4);
    if (h < 0)
    {
        h = 0;
    }
    else if (h > 419430400)
    {
        h = 419430400;
    }
    /* Quitados los 12 bits fraccionarios queda en Q22.10: % por 1024. */
    const uint32_t q22_10 = (uint32_t)(h >> 12);
    return (q22_10 * 100U + 512U) >> 10;
}

esp_err_t bme280_leer(bme280_t *s, bme280_medida_t *m)
{
    ESP_RETURN_ON_FALSE(s && s->manejador && m, ESP_ERR_INVALID_STATE, TAG,
                        "el sensor no esta colgado del bus");

    /* Modo forzado: una conversión y vuelta al reposo. */
    ESP_RETURN_ON_ERROR(escribir_registro(s, REG_CTRL_MEAS, CTRL_MEAS_FORZADO), TAG, "forzado");
    ESP_RETURN_ON_ERROR(esperar_conversion(s), TAG, "conversion");

    uint8_t d[8];
    ESP_RETURN_ON_ERROR(leer_registros(s, REG_DATOS, d, sizeof d), TAG, "datos");
    const int32_t adc_p = (int32_t)(((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | (d[2] >> 4));
    const int32_t adc_t = (int32_t)(((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | (d[5] >> 4));
    const int32_t adc_h = (int32_t)(((uint32_t)d[6] << 8) | d[7]);

    int64_t t_fine;
    m->temperatura_centigrados = compensar_temperatura(&s->calibracion, adc_t, &t_fine);
    m->presion_pa = compensar_presion(&s->calibracion, adc_p, t_fine);
    m->humedad_centiporcentaje = compensar_humedad(&s->calibracion, adc_h, t_fine);
    return ESP_OK;
}
