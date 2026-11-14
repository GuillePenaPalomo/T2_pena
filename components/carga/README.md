# Componente `carga`

Dos tareas de cálculo, `carga_a` y `carga_b`, fijadas al mismo núcleo, y una
tarea `informe`, fijada al otro, que escribe cada segundo cuánto ha avanzado
el contador de cada una. Es la práctica P1 del taller, y las demás prácticas
lo usan para medir con carga.

Cada tarea de carga repite un ciclo: una **ráfaga de cálculo**, que es una
espera activa medida con el reloj, y una **espera bloqueada** más larga que la
ráfaga. Mientras calcula, cuenta vueltas: si otra tarea le quita el
procesador, da menos, así que la cuenta dice cuánto procesador ha recibido.

| Modo | `carga_a` | `carga_b` | Qué se ve |
|---|---|---|---|
| `parada` | bloqueada | bloqueada | Nada: las dos esperan un aviso sin plazo. |
| `turnos` | prioridad base | prioridad base | Los dos contadores avanzan: el planificador las turna en cada tick. |
| `inanicion` | prioridad alta, **sin espera** | prioridad base | El contador de `carga_b` se para y, a los 5 s, avisa el watchdog de tareas. |
| `corregida` | prioridad alta, con espera | prioridad base | Los dos vuelven a avanzar y el aviso deja de salir. |

**No decide prioridades ni núcleos**: los recibe al iniciarse. En el código de
`main`:

```c
#include "carga.h"

const carga_config_t config = {
    .rafaga_ms = 20,
    .espera_ms = 27,
    .prioridad = 2,
    .prioridad_alta = 3,
    .nucleo = 0,
    .pila = 3072,
    .prioridad_informe = 5,
    .nucleo_informe = 1,
    .pila_informe = 3072,
};
ESP_ERROR_CHECK(carga_iniciar(&config));
ESP_ERROR_CHECK(carga_poner_modo(CARGA_TURNOS));
```

- `carga_iniciar()` crea las tres tareas con `xTaskCreatePinnedToCore()`. La
  carga empieza parada. Devuelve `ESP_ERR_INVALID_ARG` si la espera no es
  mayor que la ráfaga: con una ráfaga tan larga como la espera, y las dos
  tareas desfasadas, podría haber siempre una lista, la tarea inactiva no
  llegaría a ejecutarse y el aviso del watchdog saldría también en turnos.
- `carga_poner_modo()` cambia el modo y, con `vTaskPrioritySet()`, la
  prioridad de `carga_a`. La puede llamar cualquier tarea.
- `carga_modo()` y `carga_generacion()` son para quien mide con carga: la
  generación cambia con cada cambio de modo, y quien mide descarta lo
  acumulado cuando ve que ha cambiado.
- `carga_informar(false)` calla el informe de cada segundo. Con la carga
  parada no se escribe nada.

En inanición, `carga_a` vuelve a mirar el modo al acabar cada ráfaga: por eso
sale de la inanición aunque no se bloquee nunca.

Depende de `freertos`, en `REQUIRES` porque sus tipos aparecen en la cabecera
pública, y de `esp_timer`, en `PRIV_REQUIRES`.
