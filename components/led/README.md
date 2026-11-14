# Componente `led`

El brillo de un LED con una **salida modulada de LEDC**, el periférico de
modulación por ancho de pulso del ESP32-S3. Es la práctica P3 del taller. En
el Taller 1 este componente encendía y apagaba el pin como salida digital;
ahora el pin pasa a LEDC y el componente ya no lo configura como GPIO.

Un temporizador de LEDC fija la **frecuencia** y la **resolución**; un canal
saca la señal por el pin con un **ciclo de trabajo**, en pasos del
temporizador, de 0 a 2^resolución.

**Una sola tarea maneja LEDC**: la tarea `led`. Las demás le mandan órdenes por
una cola: el pulsador pide «modo siguiente» y la consola, un modo o un ciclo de
trabajo. Así ninguna llamada a LEDC se hace desde una rutina de interrupción,
ni desde dos tareas a la vez, y quien manda la orden no se queda esperando si
hay una transición en curso.

| Modo | Ciclo de trabajo |
|---|---|
| `apagado` | 0 |
| `medio` | el porcentaje configurado, 25 % por omisión |
| `encendido` | 2^resolución, el 100 % |
| `transicion` | sube y baja solo, por hardware |
| `manual` | el que fije `led_poner_ciclo()` |

El pulsador recorre los cuatro primeros en orden.

```c
#include "led.h"
#include "pines.h"

const led_config_t config = {
    .pin = PIN_LED,
    .nivel_encendido = LED_NIVEL_ENCENDIDO,
    .frecuencia_hz = 5000,
    .resolucion_bits = 13,
    .medio_porcentaje = 25,
    .transicion_ms = 1500,
    .prioridad = 4,
    .nucleo = 0,
    .pila = 3072,
};
ESP_ERROR_CHECK(led_iniciar(&config));
led_siguiente_modo();   /* deja la orden en la cola y vuelve */
```

- `led_iniciar()` configura el temporizador con `ledc_timer_config()`, en
  `LEDC_LOW_SPEED_MODE`, el único del ESP32-S3; el canal con
  `ledc_channel_config()`; instala el servicio de transiciones con
  `ledc_fade_func_install()` y registra el aviso de fin de transición. Si el
  LED está montado a 3,3 V, invierte la salida.
- **La frecuencia y la resolución no se eligen por separado**: el reloj del
  temporizador tiene que dar al menos frecuencia × 2^resolución ciclos por
  segundo, y el más rápido es de 80 MHz. Si no caben, `ledc_timer_config()`
  falla y `led_iniciar()` devuelve su error.
- El 100 % se escribe como 2^resolución, salvo con 14 bits, la resolución
  máxima, en que el contador interno se desbordaría: entonces es 2^14 − 1.
- La transición se pide con `ledc_set_fade_time_and_start(...,
  LEDC_FADE_NO_WAIT)`. Al terminar, LEDC llama a una función desde su rutina
  de interrupción, que solo avisa a la tarea por la cola; la tarea empieza la
  transición contraria. El tiempo real puede apartarse del pedido, hasta la
  mitad o el doble, porque el hardware cambia el ciclo a saltos enteros.
- Antes de cambiar el ciclo de trabajo, la tarea detiene la transición en
  curso con `ledc_fade_stop()`. Si no, `ledc_set_duty_and_update()` esperaría
  a que terminara.

Depende de `esp_driver_gpio` y `freertos`, en `REQUIRES` porque sus tipos
aparecen en la cabecera pública, y de `esp_driver_ledc`, en `PRIV_REQUIRES`.
