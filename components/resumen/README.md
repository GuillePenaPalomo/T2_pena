# Componente `resumen`

El resumen del periodo de una actividad periódica: con los instantes de un
número fijo de activaciones, calcula el intervalo medio, el mínimo, el máximo,
el **jitter** (máximo menos mínimo) y los **rebases** del periodo.

Lo usan la tarea del sensor de P2 y la tarea de medida de los temporizadores
de P4, que así miden de la misma forma.

```c
#include "resumen.h"

static resumen_t r;
resumen_iniciar(&r, 50);              /* 50 intervalos: 51 activaciones */

for (;;) {
    const bool tardia = (xTaskDelayUntil(&ref, periodo) == pdFALSE);
    const int64_t ahora = esp_timer_get_time();  /* antes de hacer nada más */
    resumen_datos_t d;
    if (resumen_anotar(&r, ahora, tardia, &d)) {
        resumen_escribir("sensor", &d, 1000000);
    }
    /* ... el trabajo de la activación ... */
}
```

- El instante se toma **al volver de la espera y antes del trabajo**. Si se
  tomara después, el intervalo sumaría la variación del propio trabajo.
- `resumen_anotar()` solo escribe en la memoria del resumen: no traza. Cuando
  completa un resumen lo copia en `datos` y empieza otro **desde esa misma
  activación**, así que no se pierde ninguna entre un resumen y el siguiente.
- `resumen_reiniciar()` lo descarta todo, también la activación anterior. Se
  llama cuando cambia algo que invalida la medida: el modo de la carga, el
  periodo o una activación perdida. Así un resumen nunca mezcla dos
  situaciones.
- Un resumen lo usa una sola tarea, y por eso no se protege.

No depende de ESP-IDF ni de FreeRTOS: es C estándar.
