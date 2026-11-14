# Componente `bme280`

El sensor BME280 como dispositivo del bus I2C: lo cuelga del bus, lo
identifica, lee su calibración, lo configura y devuelve **medidas
compensadas** de temperatura, presión y humedad.

La calibración, la configuración y las fórmulas de compensación proceden del
Taller 1 (`main/sensor_bme280.c` de aquel proyecto). Aquí se han trasladado al
componente, para que la tarea periódica del sensor solo tenga que pedir una
medida. Las fórmulas son las enteras de la hoja de datos de Bosch Sensortec,
apartado 4.2.3, «Compensation formulas».

Según la hoja de datos, la dirección es 0x76 con la patilla SDO a masa y 0x77
con SDO a la alimentación, y el registro de identificación, en 0xD0, vale 0x60.

No crea el bus: lo recibe ya creado. En el código de `main`:

```c
#include "bme280.h"

static bme280_t sensor;

ESP_ERROR_CHECK(bme280_iniciar(&sensor, bus.manejador, 0x76, 100000));
ESP_ERROR_CHECK(bme280_configurar(&sensor));

bme280_medida_t m;
if (bme280_leer(&sensor, &m) == ESP_OK) {
    /* m.temperatura_centigrados, m.presion_pa, m.humedad_centiporcentaje */
}
```

- `bme280_iniciar()` cuelga el sensor del bus en una dirección de 7 bits, sin
  desplazar. Si ya está colgado, devuelve `ESP_ERR_INVALID_STATE`.
- `bme280_configurar()` lee la identificación y falla con
  `ESP_ERR_INVALID_RESPONSE` si no es 0x60: así se distingue un problema de
  cableado de uno de programación. Después lee la calibración y configura
  sobremuestreo x1 en las tres magnitudes, sin filtro, y lo deja en reposo.
  Escribe `ctrl_hum` antes que `ctrl_meas`, porque según la hoja de datos el
  primero solo surte efecto al escribir el segundo.
- `bme280_leer()` pide una conversión en **modo forzado**, consulta el
  registro de estado hasta que termina, bloqueándose un tick cada vez, y lee
  los ocho registros de datos de una vez. Con sobremuestreo x1 la conversión
  dura menos de 10 ms. La espera está acotada a 100 ms.
- Las medidas van en enteros: centésimas de grado, pascales y centésimas de
  por ciento. Así no hace falta coma flotante.
- `bme280_leer_id()` y `bme280_quitar()` siguen como en el ejemplo de la
  asignatura.

Depende de `esp_driver_i2c`, en `REQUIRES` porque su cabecera aparece en la
cabecera pública, y de `esp_timer`, en `PRIV_REQUIRES`.
