# Componente `pulsador`

Un pulsador en un pin de entrada digital, atendido por interrupción.

Como el componente `led`, **no sabe en qué pin está el pulsador ni cómo está
montado**: los recibe al iniciarse.

Tiene dos partes:

- **El pin**, como en el Taller 1: lo configura como entrada, activa o quita
  la resistencia de polarización interna y lee el nivel.
- **La atención**, añadida en el Taller 2 para P3: una rutina de interrupción
  mínima y una tarea, `pulsador`, que aplica el antirrebote y, por cada
  pulsación válida, llama a una función de aviso que pone quien lo usa. El
  componente no sabe qué hace esa función.

```c
#include "pulsador.h"
#include "pines.h"

static pulsador_t pulsador;

static void al_pulsar(uint32_t n, void *arg)
{
    led_siguiente_modo();   /* se ejecuta en la tarea, no en la interrupción */
}

ESP_ERROR_CHECK(pulsador_iniciar(&pulsador, PIN_PULSADOR, PULSADOR_NIVEL_PULSADO));
ESP_ERROR_CHECK(pulsador_polarizar(&pulsador, false));  /* resistencia externa */
const pulsador_atencion_t atencion = {
    .guarda_ms = 200, .confirmacion_ms = 20,
    .prioridad = 6, .nucleo = 0, .pila = 3072,
    .aviso = al_pulsar, .argumento = NULL,
};
ESP_ERROR_CHECK(pulsador_atender(&pulsador, &atencion));
```

**Qué hace la rutina de interrupción.** Toma el instante del flanco con
`esp_timer_get_time()` y lo manda a la tarea con `xQueueSendFromISR()`. Nada
más: ni trazas, ni esperas, ni llamadas a LEDC. Se usa una cola y no una
notificación porque la tarea necesita el instante de cada flanco.

**Qué hace la tarea.** El antirrebote, en dos pasos:

1. Un flanco que llega antes de que pase la **guarda** desde la última
   pulsación aceptada es rebote: se descarta.
2. Si llega después, la tarea espera la **confirmación** y comprueba que el
   pin sigue pulsado. Sin esa comprobación, los rebotes al soltar una
   pulsación más larga que la guarda llegarían con la guarda ya vencida y
   contarían como otra pulsación.

Después traza la pulsación y llama a la función de aviso.

- `pulsador_atender()` instala el servicio de interrupciones de GPIO si nadie
  lo ha instalado, crea la cola y la tarea y registra la rutina con
  `gpio_isr_handler_add()`, que activa la interrupción del pin. Solo admite un
  pulsador atendido. La confirmación tiene que ser menor que la guarda.
- `pulsador_cuentas()` da las pulsaciones válidas y los flancos descartados.
- `pulsador_polarizar(&pulsador, false)` quita la resistencia interna: en el
  montaje del taller el nivel de reposo lo fija la externa.

Depende de `esp_driver_gpio` y `freertos`, en `REQUIRES` porque sus tipos
aparecen en la cabecera pública, y de `esp_timer`, en `PRIV_REQUIRES`.
