# Componente `inspeccion`

La lista de tareas, el reparto del procesador y la pila de cada tarea propia,
**escritos por el programa**. Lo pide el comando `tareas` de la consola.

`inspeccion_escribir()` escribe:

1. La salida de `vTaskList()`: estado, prioridad, pila mínima libre, número y
   núcleo de cada tarea del sistema.
2. La de `vTaskGetRunTimeStats()`: tiempo de procesador de cada tarea **desde
   el arranque**, que mezcla todos los modos usados.
3. La **carga de cada núcleo desde la llamada anterior**: 100 % menos lo que se
   ha ejecutado su tarea inactiva en ese intervalo, con
   `uxTaskGetSystemState()`. Es la comparación de dos capturas de U3-S6 hecha
   por el programa: dos llamadas separadas unos segundos con la carga en un
   modo dan la carga de ese modo. El contador es de 32 bits en microsegundos:
   entre las dos no pueden pasar más de unos 71 minutos.
4. De cada tarea propia, la prioridad, el núcleo, la pila asignada y la
   **marca de agua** medida con `uxTaskGetStackHighWaterMark()`, que en
   ESP-IDF da bytes.

```c
static const inspeccion_tarea_t propias[] = {
    {"carga_a", 3072},
    {"sensor", 3072},
};
ESP_ERROR_CHECK(inspeccion_iniciar(propias, 2));
inspeccion_escribir();
```

El búfer de `vTaskList()` necesita unos 40 bytes por tarea; se usan 2048,
estáticos, así que no la pueden llamar dos tareas a la vez.

Necesita las cuatro opciones de inspección de `sdkconfig.defaults`. Depende de
`heap`, en `PRIV_REQUIRES`.
