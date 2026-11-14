# Componente `temporizadores`

Una **alarma periódica de `esp_timer`** y un **temporizador software de
FreeRTOS**, con el mismo periodo, y la medida del jitter de cada uno. Es la
práctica P4 del taller: dos formas de hacer algo periódico sin una tarea que
espere.

| | Alarma de `esp_timer` | Temporizador software |
|---|---|---|
| Se crea con | `esp_timer_create()` y `esp_timer_start_periodic()` | `xTimerCreate()` y `xTimerStart()` |
| Cuenta en | microsegundos | ticks (`pdMS_TO_TICKS()`) |
| Ejecuta la función de respuesta | la tarea de `esp_timer`, prioridad 22, núcleo 0 | la tarea de servicio `Tmr Svc`, prioridad 1, núcleo 0 |
| Cambio de periodo | `esp_timer_restart()` | `xTimerChangePeriod()` |

La tarea de servicio no tiene afinidad por omisión. Se fija al núcleo 0, el de
la carga, con `CONFIG_FREERTOS_TIMER_TASK_AFFINITY_CPU0=y` en
`sdkconfig.defaults`: si no, se ejecutaría en el otro núcleo, la carga no la
retrasaría y la comparación no enseñaría nada.

**Cómo se mide.** Las dos funciones de respuesta son cortas y no se bloquean:
toman el instante con `esp_timer_get_time()` y lo mandan, con su origen, a la
tarea `medida` por una cola, con `xQueueSend()` y espera 0. No es una rutina de
interrupción: las dos se ejecutan en una tarea, así que no hace falta el sufijo
`FromISR`. No se usa una notificación porque lleva como mucho 32 bits, se
sobrescribe si llega otra antes de leerla y no dice de qué temporizador viene;
el instante ocupa 64.

La tarea `medida`, en el núcleo 1, calcula con el componente `resumen` un
resumen por temporizador, del mismo número de intervalos que P2. Empieza la
medida de nuevo:

- cuando cambia el modo de la carga, para que un resumen no mezcle dos modos;
- cuando cambia el periodo;
- cuando un envío falla porque la cola está llena: un instante perdido junta
  dos intervalos en uno. Los envíos perdidos se cuentan.

Si la tarea de medida tomara el instante al despertar, su propio retraso
entraría en la medida: por eso lo toma la función de respuesta.

Escribe solo el primer resumen de cada medida; con un periodo de 10 ms sale uno
cada medio segundo. `temporizadores_ultimo_resumen()` da el último de cada
temporizador y cada modo, con el periodo con que se midió.

```c
const temporizadores_config_t config = {
    .periodo_ms = 10,
    .intervalos = 50,
    .prioridad_medida = 7,
    .nucleo_medida = 1,
    .pila_medida = 3072,
};
ESP_ERROR_CHECK(temporizadores_iniciar(&config));
temporizadores_poner_periodo(13);
```

Depende de `carga`, `resumen` y `freertos`, en `REQUIRES`, y de `esp_timer`, en
`PRIV_REQUIRES`.
