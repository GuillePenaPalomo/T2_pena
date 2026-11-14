/* bme280.h - el sensor BME280 como dispositivo del bus I2C.
 *
 * Cuelga el dispositivo del bus en su dirección, lo identifica, lee sus
 * coeficientes de calibración y lo configura. Después, cada lectura pide una
 * conversión en modo forzado, espera a que termine y compensa los valores en
 * bruto con los coeficientes del propio sensor: lo que devuelve ya son
 * medidas.
 *
 * Direcciones, registros y fórmulas de compensación, según la hoja de datos
 * del BME280 de Bosch Sensortec: 0x76 con la patilla SDO a masa y 0x77 con
 * SDO a la alimentación. El registro de identificación está en 0xD0 y vale
 * 0x60. Las fórmulas son las enteras del apartado 4.2.3, «Compensation
 * formulas»; la calibración y la lectura proceden del Taller 1 de la pareja.
 */
#pragma once

#include <stdint.h>
#include "driver/i2c_master.h"

#define BME280_REG_ID 0xD0
#define BME280_ID 0x60

/* Coeficientes de calibración, grabados en el sensor en fábrica. */
typedef struct
{
    uint16_t t1;
    int16_t t2;
    int16_t t3;
    uint16_t p1;
    int16_t p2;
    int16_t p3;
    int16_t p4;
    int16_t p5;
    int16_t p6;
    int16_t p7;
    int16_t p8;
    int16_t p9;
    uint8_t h1;
    int16_t h2;
    uint8_t h3;
    int16_t h4;
    int16_t h5;
    int16_t h6;
} bme280_calibracion_t;

typedef struct
{
    i2c_master_dev_handle_t manejador; /* NULL mientras no está colgado */
    uint8_t direccion;                 /* de 7 bits, sin desplazar       */
    uint32_t velocidad_hz;
    bme280_calibracion_t calibracion;  /* válida tras bme280_configurar() */
} bme280_t;

/* Una medida compensada, en enteros para no depender de la coma flotante. */
typedef struct
{
    int32_t temperatura_centigrados; /* centésimas de grado: 2153 son 21,53 °C */
    uint32_t presion_pa;             /* pascales: 94012 son 940,12 hPa */
    uint32_t humedad_centiporcentaje; /* centésimas de %: 4512 son 45,12 % */
} bme280_medida_t;

/* Cuelga el sensor del bus en la dirección dada, de 7 bits y sin desplazar:
 * el controlador añade el bit de sentido. Si ya está colgado, no hace nada y
 * devuelve ESP_ERR_INVALID_STATE. */
esp_err_t bme280_iniciar(bme280_t *s, i2c_master_bus_handle_t bus,
                         uint8_t direccion, uint32_t velocidad_hz);

/* Descuelga el sensor del bus. */
esp_err_t bme280_quitar(bme280_t *s);

/* Lee el registro de identificación: escribe su dirección y lee un byte, sin
 * soltar el bus entre las dos cosas. Devuelve el error del controlador tal
 * cual, para poder enseñarlo. */
esp_err_t bme280_leer_id(bme280_t *s, uint8_t *id);

/* Comprueba la identificación, lee la calibración y configura el sensor:
 * sobremuestreo x1 en las tres magnitudes, sin filtro y en reposo hasta que
 * se pida una medida. Devuelve ESP_ERR_INVALID_RESPONSE si la identificación
 * no es la de un BME280 o la calibración no es válida. */
esp_err_t bme280_configurar(bme280_t *s);

/* Pide una conversión en modo forzado, espera a que termine, con un límite de
 * 100 ms, y compensa el resultado. Bloquea a quien la llama unos 10 ms. */
esp_err_t bme280_leer(bme280_t *s, bme280_medida_t *m);
