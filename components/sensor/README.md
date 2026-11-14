# Componente `sensor`

La tarea periódica que lee el BME280 y mide su propio periodo. Es la práctica
P2 del taller.

La tarea `sensor`:

1. Espera con **`xTaskDelayUntil()`**, una espera absoluta: hasta un instante
   fijo, que avanza exactamente un periodo en cada vuelta. Lo que tarda cada
   vuelta, la lectura del sensor y las trazas, no se suma al periodo, como sí
   pasaría con `vTaskDelay()`, que espera una duración a partir de cuando se
   llama.
2. Nada más despertar, toma el instante con `esp_timer_get_time()`, **antes de
   leer el sensor**: así el intervalo entre dos activaciones habla solo de la
   activación, no de lo que varía la lectura.
3. Lo anota en un resumen de un número fijo de intervalos (componente
   `resumen`). Cuenta como rebase cada vez que `xTaskDelayUntil()` devuelve
   `pdFALSE`, que es cuando el instante siguiente ya había pasado.
4. Lee el sensor con el componente `bme280` y guarda la medida.
5. Cuando el resumen se completa, lo escribe y lo guarda como el último del
   modo de la carga con el que se midió. **Solo escribe el resumen**, no una
   traza por activación, que alteraría lo que se mide.

**Un resumen, un solo modo de la carga.** En cada activación la tarea compara
`carga_generacion()` con la que vio al empezar. Si ha cambiado, descarta lo
acumulado y empieza de nuevo. Lo mismo si cambia su propia prioridad.

```c
#include "sensor.h"

const sensor_config_t config = {
    .bme280 = &bme280,      /* ya configurado */
    .periodo_ms = 1000,
    .intervalos = 50,
    .prioridad = 4,
    .nucleo = 0,            /* el de la carga, para que la carga pueda retrasarla */
    .pila = 3072,
};
ESP_ERROR_CHECK(sensor_iniciar(&config));
```

- `sensor_ultima_medida()` y `sensor_ultimo_resumen()` copian lo último que
  ha escrito la tarea, dentro de una sección crítica. Las usan los comandos de
  la consola.
- `sensor_poner_prioridad()` cambia la prioridad de la tarea en ejecución y
  empieza la medida de nuevo. Sirve para la extensión de P2: invertir la
  relación de prioridades con la carga y volver a medir.

Depende de `bme280`, `carga`, `resumen` y `freertos`, en `REQUIRES` porque sus
tipos aparecen en la cabecera pública, y de `esp_timer`, en `PRIV_REQUIRES`.
