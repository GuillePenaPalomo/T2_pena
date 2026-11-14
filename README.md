# Taller 2 — FreeRTOS: tareas, prioridades y temporización

SEyTR · Máster Universitario en Robótica y Automática · UCLM · Curso 2026-27

**Integrante:** Guillermo De La Peña Palomo. Trabajo individual: el cableado,
la codificación, las pruebas y la documentación son de la misma persona.

Proyecto ESP-IDF v6.1 para la placa ESP32-S3-DevKitC-1 con las cuatro prácticas
del taller en un mismo programa: dos tareas de carga y la inanición (P1), una
tarea periódica que lee el BME280 y mide su jitter (P2), el brillo del LED con
LEDC y el pulsador (P3) y una alarma de `esp_timer` frente a un temporizador
software (P4). Todo se maneja desde una consola por la UART0.

## Índice

1. [Montaje y pines](#1-montaje-y-pines)
2. [Compilar y grabar](#2-compilar-y-grabar)
3. [Organización del proyecto](#3-organización-del-proyecto)
4. [Tabla de tareas](#4-tabla-de-tareas)
5. [P1 · Tareas y prioridades](#5-p1--tareas-y-prioridades)
6. [P2 · Tarea periódica con el sensor](#6-p2--tarea-periódica-con-el-sensor)
7. [P3 · Salida modulada con LEDC](#7-p3--salida-modulada-con-ledc)
8. [P4 · Temporizadores](#8-p4--temporizadores)
9. [Lista de tareas y reparto del procesador](#9-lista-de-tareas-y-reparto-del-procesador)
10. [Comandos de la consola](#10-comandos-de-la-consola)
11. [Justificaciones](#11-justificaciones)
12. [Qué se hizo en el taller y qué después](#12-qué-se-hizo-en-el-taller-y-qué-después)
13. [Fuentes y herramientas de asistencia](#13-fuentes-y-herramientas-de-asistencia)

## 1. Montaje y pines

Los pines se declaran **solo** en [`main/pines.h`](main/pines.h). Los
componentes no lo incluyen: `main` les pasa el pin al iniciarlos.

| Función | Pin | Nivel activo | Montaje |
|---|---|---|---|
| LED | `GPIO_NUM_4` | Encendido: `1` | LED y resistencia limitadora en serie, entre el pin y GND. En este taller lo gobierna LEDC. |
| Pulsador | `GPIO_NUM_5` | Pulsado: `0` | Pulsador entre el pin y GND, con resistencia de pull-up externa a 3V3 en la protoboard. La pull-up interna se activa también, en paralelo (ver P3). |
| I2C SDA (datos) | `GPIO_NUM_8` | — | SDA del BME280 |
| I2C SCL (reloj) | `GPIO_NUM_9` | — | SCL del BME280 |

El BME280 se alimenta a **3V3** y comparte la masa con la placa. Su placa lleva
las resistencias de pull-up del bus, así que las internas del ESP32-S3 están
desactivadas. Responde en la dirección `0x76` (SDO a masa) con la
identificación `0x60`. La placa se conecta por el conector **USB-a-UART**, que
es el de la consola (UART0, 115200 baudios).

Todo el montaje trabaja a 3V3, y cualquier cambio de cableado se hace con la
placa desconectada del USB.

## 2. Compilar y grabar

Con la extensión de ESP-IDF de VS Code, o desde un terminal de ESP-IDF v6.1:

```text
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

`set-target` genera el `sdkconfig` a partir de
[`sdkconfig.defaults`](sdkconfig.defaults), que es lo único que se versiona de
la configuración:

```text
CONFIG_IDF_TARGET="esp32s3"
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y
CONFIG_FREERTOS_TIMER_TASK_AFFINITY_CPU0=y
```

Comprobado en SDK Configuration Editor, *Component config → FreeRTOS →
Kernel*: `configTICK_RATE_HZ` = 1000, `configGENERATE_RUN_TIME_STATS` activada
y `configTIMER_SERVICE_TASK_CORE_AFFINITY` = CPU0.

> Hasta el 14-11 el fichero se llamaba `sdkconfig.default`, sin la `s`, y
> ESP-IDF no lo leía: el tick estaba a 100 Hz y la inspección de FreeRTOS,
> desactivada. Se renombró y se volvió a fijar el destino.

Los parámetros del taller están en el menú **SEyTR - Taller 2** de
`menuconfig` ([`main/Kconfig.projbuild`](main/Kconfig.projbuild)):

| Opción | Valor | Para qué |
|---|---:|---|
| `SEYTR_CARGA_RAFAGA_MS` | 20 | Cálculo de cada ciclo de carga |
| `SEYTR_CARGA_ESPERA_MS` | 27 | Espera bloqueada de cada ciclo de carga |
| `SEYTR_SENSOR_PERIODO_MS` | 1000 | Periodo de muestreo del sensor |
| `SEYTR_SENSOR_INTERVALOS` | 50 | Intervalos de cada resumen (P2 y P4) |
| `SEYTR_TEMPORIZADORES_PERIODO_MS` | 10 | Periodo inicial de los dos temporizadores |
| `SEYTR_LED_FRECUENCIA_HZ` | 5000 | Frecuencia de LEDC |
| `SEYTR_LED_RESOLUCION_BITS` | 13 | Resolución de LEDC |
| `SEYTR_LED_MEDIO_PORCENTAJE` | 25 | Brillo del modo medio |
| `SEYTR_LED_TRANSICION_MS` | 1500 | Duración pedida de cada subida o bajada |
| `SEYTR_GUARDA_MS` | 200 | Guarda del antirrebote, la del Taller 1 |
| `SEYTR_CONFIRMACION_MS` | 20 | Espera antes de confirmar el nivel del pulsador |

## 3. Organización del proyecto

```text
T2_pena/
├── main/
│   ├── main.c            solo inicia y arranca
│   ├── pines.h           los pines, en un solo sitio
│   ├── tareas.h          prioridad, núcleo y pila de cada tarea, en un solo sitio
│   └── Kconfig.projbuild el menú SEyTR - Taller 2
├── components/
│   ├── carga/            P1: dos tareas de carga, sus modos y el informe
│   ├── resumen/          P2 y P4: medio, mínimo, máximo, jitter y rebases
│   ├── bme280/           P2: el sensor, con lectura compensada (del Taller 1)
│   ├── bus_i2c/          el bus I2C (del Taller 1)
│   ├── sensor/           P2: la tarea periódica del sensor
│   ├── led/              P3: el LED con LEDC (reescrito)
│   ├── pulsador/         P3: el pulsador con interrupción y antirrebote (ampliado)
│   ├── temporizadores/   P4: alarma de esp_timer y temporizador software
│   ├── inspeccion/       vTaskList, vTaskGetRunTimeStats y marcas de agua
│   ├── consola/          la consola por UART (del Taller 1, sin cambios)
│   └── comandos/         la tabla de comandos de la consola
├── resultados/           las trazas capturadas de la placa
├── sdkconfig.defaults
└── AUTOEVALUACION.md
```

Cada componente tiene su `README.md` con su interfaz y sus decisiones, su
etiqueta de traza y su `CMakeLists.txt` con `REQUIRES` o `PRIV_REQUIRES`. Los
componentes no saben en qué pin está su periférico ni con qué prioridad corre
su tarea: lo reciben de `main`, que lo toma de `pines.h` y `tareas.h`.

## 4. Tabla de tareas

Marca de agua medida con `uxTaskGetStackHighWaterMark()`, que en ESP-IDF da
bytes, con todo el programa en marcha, la carga en turnos y el LED en
transición ([`resultados/tareas_y_reparto.txt`](resultados/tareas_y_reparto.txt)).

| Tarea | Prio | Periodo | Núcleo | Pila (B) | Mín. libre (B) | Razón de la prioridad |
|---|---:|---|---:|---:|---:|---|
| `carga_a` | 2 (3 en inanición y corregida) | ciclo de 47 ms: 20 de cálculo + 27 de espera | 0 | 3072 | 2376 | Por encima de 1, la tarea de servicio de temporizadores, para retrasarla en P4; por debajo de todas las propias y de la consola, para que la consola responda aunque acapare el núcleo. En inanición sube a 3 para pasar delante de `carga_b`. |
| `carga_b` | 2 | ídem | 0 | 3072 | 2368 | Igual que `carga_a` en turnos. |
| `informe` | 5 | 1 s | 1 | 3072 | 2392 | Solo escribe una línea por segundo; va en el núcleo 1 para seguir escribiendo con el núcleo 0 acaparado. |
| `sensor` | 4 | 1 s | 0 | 3072 | 1616 | Más que la carga: si tuviera menos, cada activación que cayera en una ráfaga esperaría hasta su final (P2). Menos que la consola. |
| `led` | 4 | por evento: órdenes por cola | 0 | 3072 | 1712 | Más que la carga, para que el cambio de modo no espere al final de una ráfaga; menos que el pulsador, que le manda la orden. |
| `pulsador` | 6 | por evento: interrupción | 0 | 3072 | 2340 | El plazo más corto de las propias del núcleo 0: una pulsación se tiene que notar en decenas de ms, y su trabajo es mínimo. |
| `medida` | 7 | por evento: 2 instantes cada 10 ms | 1 | 3072 | 1328 | La que más trabajo recibe; en el núcleo 1, para que la carga no la retrase y no se llene la cola. Su retraso no entra en la medida: el instante lo toma la función de respuesta. |
| `consola` | 5 | por evento: UART | sin afinidad | 4096 | 2612 | La fija su componente, del Taller 1. Por encima de la carga, para responder en inanición. |

Tareas del sistema que intervienen: `Tmr Svc` (1, núcleo 0 por
`sdkconfig.defaults`), `esp_timer` (22, núcleo 0), `IDLE0` e `IDLE1` (0).

Todas las propias tienen más de 1300 B libres en el peor caso medido: las pilas
de 3072 B dejan margen y se podrían reducir, pero se dejan así porque `printf`
con varios argumentos y las llamadas a LEDC o I2C pueden usar más en caminos
que la medida no ha recorrido.

## 5. P1 · Tareas y prioridades

**Lo construido.** Componente [`carga`](components/carga): `carga_a` y
`carga_b`, creadas con `xTaskCreatePinnedToCore()` **las dos en el núcleo 0**.
Cada una repite un ciclo: 20 ms de cálculo, medidos en tiempo de reloj, y 27 ms
de espera bloqueada con `vTaskDelay()`. Mientras calcula, cuenta vueltas; una
tarea `informe`, en el núcleo 1, escribe cada segundo cuánto ha avanzado cada
contador. El comando `carga` cambia entre cuatro modos sin recompilar:

| Modo | `carga_a` | `carga_b` |
|---|---|---|
| `parada` | bloqueada sin plazo | bloqueada sin plazo |
| `turnos` | prio 2, con espera | prio 2, con espera |
| `inanicion` | prio 3, **sin espera** | prio 2, con espera |
| `corregida` | prio 3, con espera | prio 2, con espera |

La prioridad se cambia en ejecución con `vTaskPrioritySet()`. La espera es mayor
que la ráfaga (un `_Static_assert` en `main.c` lo comprueba): si no, con las dos
tareas desfasadas podría haber siempre una lista y el aviso del watchdog saldría
también en turnos. El ciclo, 47 ms, no divide el periodo del sensor (1000 ms) ni
es múltiplo del de los temporizadores (10 ms); también se comprueba al compilar.

**Resultados** ([`resultados/p1_turnos_inanicion_corregida.txt`](resultados/p1_turnos_inanicion_corregida.txt)):

```text
carga turnos (t = 6279 ms): carga_a prio 2 avanza 194653; carga_b prio 2 avanza 213984
carga inanicion (t = 10279 ms): carga_a prio 3 avanza 931713; carga_b prio 2 avanza 0 (PARADA: no recibe procesador)
E (13728) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (13728) task_wdt:  - IDLE0 (CPU 0)
E (13728) task_wdt: Tasks currently running:
E (13728) task_wdt: CPU 0: carga_a
E (13728) task_wdt: CPU 1: IDLE1
carga corregida (t = 20279 ms): carga_a prio 3 avanza 391839; carga_b prio 2 avanza 402981
```

**Explicación.**

- **Turnos.** Con la misma prioridad y el mismo núcleo, el planificador las
  turna en cada tick (U3-S3): las dos avanzan, cada una unas 200 000 vueltas
  por segundo. Las dos empiezan su ráfaga casi a la vez, así que se reparten el
  núcleo durante 20 ms y después las dos se bloquean 27 ms.
- **Inanición.** `carga_a` tiene más prioridad y no se bloquea nunca: siempre
  está lista, y el planificador elige siempre la tarea lista de más prioridad.
  `carga_b` está lista pero no recibe procesador, y su contador se para. Tampoco
  se ejecuta `IDLE0`, la tarea inactiva del núcleo 0, de prioridad 0. El aviso
  sale 5,0 s después del cambio de modo (8716 → 13728 ms), el plazo del
  watchdog de tareas con la configuración de partida. Se lee así: la primera
  parte nombra la tarea que no le ha dado señal, `IDLE0 (CPU 0)`; la segunda,
  la que ocupaba cada núcleo: `carga_a` en el 0, que es la que no deja
  ejecutarse a la inactiva, e `IDLE1` en el 1, que está libre. El programa
  sigue: por omisión el aviso no reinicia la placa.
- **Corregida.** `carga_a` sigue con más prioridad, pero vuelve a bloquearse 27
  ms en cada ciclo. Mientras está bloqueada se ejecutan `carga_b` y la tarea
  inactiva: los dos contadores avanzan y el aviso deja de salir. Lo que corrige
  la inanición no es la prioridad, sino que la tarea de más prioridad se
  bloquee.

La consola (prioridad 5, sin afinidad) y el informe (núcleo 1) siguen
respondiendo en inanición.

## 6. P2 · Tarea periódica con el sensor

**Lo construido.** Componente [`sensor`](components/sensor). La tarea `sensor`,
en el núcleo 0, el de la carga, espera con **`xTaskDelayUntil()`**, una espera
absoluta. Nada más despertar, **antes de leer el sensor**, toma el instante con
`esp_timer_get_time()` y lo anota en un resumen de **50 intervalos** (componente
[`resumen`](components/resumen)): intervalo medio, mínimo, máximo, jitter (máximo
menos mínimo) y rebases (las veces que `xTaskDelayUntil()` devuelve `pdFALSE`).
Después lee el BME280 con el componente [`bme280`](components/bme280), que ahora
hace la calibración y la compensación del Taller 1. Solo escribe el resumen,
una vez cada 50 activaciones, no una traza por activación.

**Un resumen, un modo de la carga.** En cada activación la tarea compara
`carga_generacion()` con la que vio al empezar; si el modo de la carga ha
cambiado, descarta lo acumulado y empieza de nuevo. El comando `sensor` da la
última medida y el último resumen de cada modo.

**Cómo se ha medido.** Con el periodo decidido, 1 s, y 50 intervalos: cada
resumen tarda 50 s. Se puso la carga en `parada`, se esperó un resumen, se puso
en `turnos` y se esperó otro. Como extensión, se bajó la prioridad del sensor a
1 con `sensor prio 1` y se midió otra vez con carga.

**Resultados** ([`resultados/p2_sensor_sin_y_con_carga.txt`](resultados/p2_sensor_sin_y_con_carga.txt)):

| Carga | Prio sensor | Medio | Mín | Máx | Jitter | Rebases |
|---|---:|---:|---:|---:|---:|---:|
| parada | 4 | 999,999 ms | 999,998 | 1000,000 | **0,002 ms** | 0 |
| turnos | 4 | 999,999 ms | 999,998 | 1000,001 | **0,003 ms** | 0 |
| turnos (extensión) | 1 | 999,679 ms | 986,975 | 1021,014 | **34,039 ms** | 0 |

Lectura del sensor en esos momentos: T = 23,8 °C, P = 957,8 hPa, HR = 52-56 %.

**Explicación.**

- **Por qué la espera absoluta mantiene el periodo.** `xTaskDelayUntil()` espera
  hasta un instante fijo que avanza exactamente un periodo cada vuelta. Lo que
  tarda la vuelta, unos 10 ms de lectura por I2C, no se suma al periodo, como
  pasaría con `vTaskDelay(1000)`. El intervalo medio sale 999,999 ms: el periodo
  pedido.
- **Qué cambia con la carga.** Con prioridad 4, más que la carga (2), nada: en
  cuanto el tick despierta al sensor, este expulsa a la tarea de carga que
  estuviera calculando. El jitter pasa de 2 a 3 µs.
- **La extensión lo confirma.** Con prioridad 1, por debajo de la carga, una
  activación que cae dentro de una ráfaga espera a que las dos tareas de carga
  se bloqueen: hasta unos 21 ms. El máximo sube a 1021 ms y, como el instante
  siguiente es fijo, el intervalo siguiente se acorta (987 ms). El jitter crece
  a 34 ms, y el medio sigue cerca de 1000 ms porque los retrasos no se acumulan.
  No hay rebases porque ningún retraso llega a un periodo.
- **Por qué esa prioridad.** El plazo de la tarea es su periodo, 1 s, pero lo
  que interesa es que cada muestra se tome a su hora: con más prioridad que la
  carga, el retraso es de microsegundos. No necesita más que el pulsador o la
  consola, que tienen plazos más cortos.

**Incidencia.** En la primera captura el BME280 no respondía:
`i2c_master_probe()` agotaba el tiempo en todas las direcciones, señal de un bus
que no funciona y no de un sensor que no contesta. Era un fallo de cableado, que
se corrigió. Desde entonces `main` explora el bus y escribe lo que encuentra
cuando el sensor no responde.

## 7. P3 · Salida modulada con LEDC

**Lo construido.** El componente [`led`](components/led) se reescribe sobre LEDC:
temporizador con `ledc_timer_config()` en `LEDC_LOW_SPEED_MODE`, el único del
ESP32-S3, a **5 kHz y 13 bits**; canal con `ledc_channel_config()` en el pin del
LED, que pasa a LEDC y ya no se configura como GPIO; servicio de transiciones
con `ledc_fade_func_install()`. **Una sola tarea, `led`, maneja LEDC**; las
demás le mandan órdenes por una cola. Cuatro modos que se recorren en orden:

| Modo | Ciclo de trabajo |
|---|---|
| apagado | 0 de 8192 |
| medio | 2048 de 8192 (25 %) |
| encendido | 8192 de 8192 (100 %: con 13 bits se escribe 2^13) |
| transición | baja de 8192 a 0 y sube, por hardware, 1,5 s cada una |

La transición se pide con `ledc_set_fade_time_and_start(..., LEDC_FADE_NO_WAIT)`.
Al terminar, LEDC llama a una función desde su interrupción que solo avisa a la
tarea por la cola, y la tarea empieza la contraria. Antes de cambiar el ciclo,
la tarea detiene la transición en curso con `ledc_fade_stop()`.

El componente [`pulsador`](components/pulsador) se amplía con la atención por
interrupción, como en el Taller 1 pero dentro del componente:

- La **rutina de interrupción** solo toma el instante del flanco y lo manda por
  una cola con `xQueueSendFromISR()`. Ni trazas, ni esperas, ni LEDC.
- La **tarea `pulsador`** aplica el antirrebote: descarta los flancos dentro de
  la guarda (200 ms) y, pasado el rebote (20 ms), confirma que el pin sigue
  pulsado. Por cada pulsación válida llama a la función de aviso que le da
  `main`, que manda «modo siguiente» a la tarea del LED.

**Resultados** ([`resultados/p3_led_modos.txt`](resultados/p3_led_modos.txt)):

```text
pulsador: pulsacion 1 (flancos descartados hasta ahora: 0)
led: modo medio, ciclo de trabajo 2048 de 8192
pulsador: pulsacion 2 (flancos descartados hasta ahora: 0)
led: modo encendido, ciclo de trabajo 8192 de 8192
pulsador: pulsacion 3 (flancos descartados hasta ahora: 0)
led: modo transicion, ciclo de trabajo 8192 de 8192, sube y baja por hardware
pulsador: pulsacion 4 (flancos descartados hasta ahora: 0)
led: modo apagado, ciclo de trabajo 0 de 8192
...
led 50
led: ciclo de trabajo 50 %, 4096 de 8192
```

Cada pulsación pasa al modo siguiente, sin saltarse ninguno, también las que se
mantuvieron un segundo; la transición se ve subir y bajar y el LED no parpadea a
la vista (comprobado mirando el LED). En la captura se pidieron 8 pulsaciones y
el contador marcó 9 con solo 2 flancos descartados; no se pudo confirmar si
hubo una pulsación de más o una contada de más.

**Por qué el ciclo de trabajo se cambia en la tarea.** Las llamadas a LEDC no
se pueden hacer en una rutina de interrupción: `ledc_set_duty_and_update()`
toma un cerrojo y, si hay una transición en curso, espera a que termine. Con una
sola tarea dueña de LEDC, nadie la llama a la vez desde dos sitios, el pulsador
y la consola no se quedan esperando y la rutina de interrupción se queda en lo
mínimo.

**Lo que no funcionó a la primera.** Con la pull-up interna desactivada, como en
el Taller 1, la primera captura contó **19 pulsaciones de 8** y unos **20 000
flancos** descartados ([`resultados/p3_led_modos_pin_flotante.txt`](resultados/p3_led_modos_pin_flotante.txt)):
la entrada estaba flotante, con toda probabilidad por un mal contacto de la
resistencia externa. Se activó la pull-up interna en paralelo con la externa
(unos 45 kΩ en paralelo con la externa no cambian el nivel de reposo) y la
captura siguiente salió limpia.

**Extensión del potenciómetro.** No se ha hecho.

## 8. P4 · Temporizadores

**Lo construido.** Componente [`temporizadores`](components/temporizadores):

- Una **alarma periódica de `esp_timer`**, con `esp_timer_create()` y
  `esp_timer_start_periodic()`, de 10 ms en microsegundos.
- Un **temporizador software de FreeRTOS**, con `xTimerCreate()` periódico y
  `xTimerStart()`, de 10 ms en ticks (`pdMS_TO_TICKS(10)`).
- Las dos funciones de respuesta son cortas y no se bloquean: toman el instante
  con `esp_timer_get_time()` y lo mandan, con su origen, a la tarea `medida`
  por una cola con `xQueueSend()` y espera 0. Una notificación no serviría:
  lleva 32 bits, se sobrescribe y no dice de qué temporizador viene.
- La tarea `medida`, en el núcleo 1, calcula con el mismo componente `resumen`
  y los mismos 50 intervalos de P2 un resumen por temporizador. Empieza de
  nuevo al cambiar el modo de la carga, al cambiar el periodo y al perderse un
  envío por cola llena, que cuenta.
- La tarea de servicio `Tmr Svc` se fija al **núcleo 0**, el de la carga, con
  `CONFIG_FREERTOS_TIMER_TASK_AFFINITY_CPU0=y`. La lista de tareas lo confirma:
  `Tmr Svc ... 1 ... 0`. La tarea de `esp_timer` ya está en el 0.
- Comandos: `periodo <ms>` cambia el de los dos con `esp_timer_restart()` y
  `xTimerChangePeriod()`; `led <0-100>` cambia el ciclo de trabajo, mandándolo a
  la tarea del LED; `jitter` escribe los últimos resúmenes.

**Resultados** ([`resultados/p4_temporizadores_sin_y_con_carga.txt`](resultados/p4_temporizadores_sin_y_con_carga.txt)):

| Temporizador | Carga | Periodo | Medio | Mín | Máx | Jitter |
|---|---|---:|---:|---:|---:|---:|
| alarma `esp_timer` | parada | 10 ms | 9,999 ms | 9,989 | 10,005 | **0,016 ms** |
| temporizador software | parada | 10 ms | 9,999 ms | 9,976 | 10,004 | **0,028 ms** |
| alarma `esp_timer` | turnos | 10 ms | 10,000 ms | 9,985 | 10,019 | **0,034 ms** |
| temporizador software | turnos | 10 ms | 9,699 ms | 0,005 | 31,131 | **31,126 ms** |
| alarma `esp_timer` | turnos | 13 ms | 12,999 ms | 12,992 | 13,016 | 0,024 ms |
| temporizador software | turnos | 13 ms | 13,000 ms | 0,009 | 34,026 | 34,017 ms |

Ningún envío perdido en estas medidas. El cambio de periodo a 13 ms y de vuelta
a 10 ms, y el cambio del ciclo del LED al 75 %, se hicieron desde la consola.

**Explicación.**

- **Sin carga, los dos se parecen**: las dos tareas que ejecutan las funciones
  de respuesta encuentran el núcleo 0 libre, y el jitter es de decenas de µs.
- **Con carga, solo empeora el temporizador software.** Su función de
  respuesta la ejecuta `Tmr Svc`, de prioridad 1, en el núcleo 0. Si vence
  durante una ráfaga, `Tmr Svc` está lista pero las tareas de carga, de
  prioridad 2, pasan delante: espera a que las dos se bloqueen, hasta unos 21
  ms. Entonces ejecuta seguidas las llamadas pendientes: de ahí intervalos de
  31 ms seguidos de otros de **5 µs**. El intervalo medio sigue siendo 10 ms
  porque el temporizador calcula cada vencimiento desde el anterior previsto, no
  desde cuando se atendió.
- **La alarma no**: la ejecuta la tarea de `esp_timer`, de prioridad 22, que
  expulsa a la carga en cuanto vence.
- Además, el temporizador software cuenta en ticks de 1 ms y la alarma en
  microsegundos: un periodo que no sea un número entero de ticks solo lo puede
  dar la alarma.
- El ciclo de la carga, 47 ms, no es múltiplo de 10 ni de 13 ms: unos
  vencimientos caen en ráfaga y otros en espera, y eso es lo que el jitter ve.

**Al salir de la inanición** el temporizador software pierde envíos: en la
[sesión de consola](resultados/sesion_consola.txt) se contaron 556, y el primer
resumen tras salir sale alterado (intervalo medio de 0,004 ms). En inanición
`Tmr Svc` no se ejecuta, y al salir ejecuta seguidas todas las llamadas
pendientes, que llenan la cola. La medida empieza de nuevo tras cada pérdida, y
el resumen siguiente ya es normal.

## 9. Lista de tareas y reparto del procesador

El comando `tareas` ([componente `inspeccion`](components/inspeccion)) escribe la
salida de `vTaskList()` y de `vTaskGetRunTimeStats()`, la carga de cada núcleo
desde la llamada anterior y la tabla de pilas. Captura con todo el programa en
marcha: dos llamadas en turnos separadas 8 s y una tercera 8 s después de parar
la carga ([`resultados/tareas_y_reparto.txt`](resultados/tareas_y_reparto.txt)).

Extracto de la lista de tareas en turnos:

```text
Nombre          Estado  Prio    Pila    Num     Nucleo
consola         X       5       2612    15      -1
carga_b         R       2       2368    9       0
carga_a         R       2       2376    8       0
IDLE0           R       0       864     5       0
Tmr Svc         B       1       1344    7       0
sensor          B       4       1616    11      0
medida          B       7       1328    14      1
esp_timer       S       22      3412    3       0
```

**Interpretación de las tareas propias.**

- `consola` está **X**, en ejecución: es la que pide la lista. Núcleo -1: sin
  afinidad.
- En turnos, `carga_a` y `carga_b` están **R**, listas: las dos están en el
  núcleo 0, que en ese instante ocupa la consola, y esperan su turno. En la
  primera captura `Tmr Svc` aparece también **R**: lista y sin procesador
  porque la carga, de más prioridad, lo ocupa. Es exactamente el retraso que
  mide P4.
- Tras parar la carga, `carga_a` y `carga_b` pasan a **S**: bloqueadas sin
  plazo, esperando la notificación de `carga_poner_modo()`.
- `sensor`, `led`, `pulsador`, `informe` y `medida` están **B**, bloqueadas
  esperando su instante o su cola: casi todo el tiempo no consumen procesador.
- La columna Pila coincide con la tabla de pilas: lo mínimo libre en bytes. La
  más justa es `medida`, con 1328 B libres de 3072.
- Núcleos: la carga, el sensor, el LED, el pulsador, `Tmr Svc` y `esp_timer`
  en el 0; `informe` y `medida` en el 1, como se diseñó.

**Carga de cada núcleo.** `vTaskGetRunTimeStats()` da el reparto **desde el
arranque**, que mezcla los modos. La carga de un modo sale de comparar dos
capturas en ese modo, que es lo que el programa hace con la tarea inactiva:

| Intervalo | Núcleo 0 | Núcleo 1 |
|---|---:|---:|
| 8,1 s en turnos | **45,5 %** (inactiva 54,5 %) | 0,4 % |
| 9,2 s tras pasar a parada | **6,0 %** (inactiva 94,0 %) | 0,5 % |

En turnos la carga no es del 85 % (2 × 20 / 47) sino de algo más del 43 % (20 /
47): las dos tareas empiezan su ráfaga a la vez y la ráfaga se mide en tiempo de
reloj, así que se reparten los mismos 20 ms en lugar de ocupar 40. El 6 % del
intervalo en parada incluye el segundo que pasó en turnos antes del cambio; sin
carga, el núcleo 0 solo ejecuta el sensor, el LED y los temporizadores. El
núcleo 1 está casi libre: el informe y la tarea de medida trabajan poco.

## 10. Comandos de la consola

Por la UART0, a 115200 baudios. Líneas de hasta 64 caracteres; una más larga se
descarta entera y se avisa; un comando desconocido o una línea vacía no rompen
nada ([`resultados/sesion_consola.txt`](resultados/sesion_consola.txt)).

| Comando | Qué hace |
|---|---|
| `help` | Lista los comandos. |
| `carga [parada\|turnos\|inanicion\|corregida]` | Cambia el modo de la carga (P1). Sin argumento, lo muestra. |
| `informe on\|off` | Muestra u oculta el informe de la carga de cada segundo. |
| `sensor` | Última medida del BME280, progreso del resumen en curso y último resumen de cada modo de la carga (P2). |
| `sensor prio <n>` | Cambia la prioridad de la tarea del sensor y empieza la medida de nuevo (extensión de P2). |
| `led` | Modo y ciclo de trabajo del LED, pulsaciones y flancos descartados. |
| `led siguiente\|apagado\|medio\|encendido\|transicion` | Pasa al modo siguiente o a uno dado (P3). |
| `led <0-100>` | Fija el ciclo de trabajo en tanto por ciento (P4). |
| `periodo [ms]` | Cambia el periodo de la alarma y del temporizador software (P4). Sin argumento, lo muestra. |
| `jitter` | Últimos resúmenes de los dos temporizadores, por modo de la carga, y envíos perdidos (P4). |
| `tareas` | Lista de tareas, reparto del procesador, carga de cada núcleo desde la llamada anterior y pilas. |

## 11. Justificaciones

**Periodo de muestreo del sensor: 1 s.** La temperatura, la presión y la humedad
de una sala cambian en segundos o minutos, así que 1 s sigue de sobra cualquier
cambio real, como acercar la mano, y deja el procesador libre: la lectura dura
unos 10 ms, el 1 % del periodo. En el Taller 1 se usaron 2 s; se baja a 1 s para
que cada resumen de 50 intervalos tarde 50 s y no 100. Con el tick a 1000 Hz son
1000 ticks exactos.

**Prioridad de cada tarea propia.** De mayor a menor plazo exigido: `medida` (7)
recibe dos instantes cada 10 ms; `pulsador` (6) tiene que responder a una
pulsación en decenas de ms; `consola` (5) e `informe` (5) tienen que responder
también con el núcleo 0 acaparado; `sensor` y `led` (4) tienen que pasar delante
de la carga para que su retraso sea de microsegundos; la carga (2) va por encima
de `Tmr Svc` (1) para retrasarla en P4 y por debajo de todo lo demás. Las de
observación (`informe`, `medida`) van en el núcleo 1. Detalle por tarea en la
[tabla de tareas](#4-tabla-de-tareas) y en [`main/tareas.h`](main/tareas.h).

**Frecuencia y resolución de LEDC: 5 kHz y 13 bits.** 5 kHz está muy por
encima de los ~100 Hz a partir de los que el ojo no distingue el parpadeo, y
también del efecto estroboscópico al mover el LED. Las dos no se eligen por
separado: el reloj del temporizador tiene que dar frecuencia × 2^resolución,
y el más rápido es de 80 MHz; a 5 kHz caben 16 000 pasos, así que 13 bits, 8192
niveles, es la máxima resolución posible. Son de sobra para que la transición
se vea continua y para el 25 % del modo medio (2048 pasos).

## 12. Qué se hizo en el taller y qué después

> **Revisar antes de entregar.** Esta sección se ha escrito a partir del
> historial de Git; corrígela con lo que se enseñó de verdad en el turno de
> demostración. Según el guion, lo que no se empezó en el taller no se puntúa.

- **Hasta el taller y el día siguiente (7 y 8 de noviembre):** proyecto creado,
  componentes del Taller 1 copiados, pines en `pines.h`, componente `bus_i2c` y
  un primer esbozo de las dos tareas de carga de P1 en `main.c`, sin modos ni
  informe que funcionara.
- **Terminado después del taller (14 de noviembre):** P1 completa (modos,
  informe, comando), P2, P3, P4, la integración en componentes, el comando
  `tareas`, las capturas de `resultados/` y esta memoria. El
  `sdkconfig.defaults` estaba mal nombrado y no se aplicaba.
- **Lo que fallaba y se corrigió:** el `sdkconfig.defaults` (tick a 100 Hz), el
  cableado del BME280 y la entrada flotante del pulsador (ver P2 y P3).
- **Lo que no se ha hecho:** la extensión del potenciómetro de P3.

## 13. Fuentes y herramientas de asistencia

- **Código de la asignatura**, reutilizado sin citar según las normas, pero
  indicado para que se pueda seguir: la estructura de la salida modulada y del
  antirrebote con confirmación sigue `salida_modulada.c` de
  [SEyTR_depuracion_de_tareas](https://github.com/SEyTR-MURA/SEyTR_depuracion_de_tareas);
  `vTaskList`, el reparto y la carga con la tarea inactiva siguen su `comun.c` y
  `carga.c`; los temporizadores siguen la variante 9 de
  [SEyTR_planificacion_de_tareas](https://github.com/SEyTR-MURA/SEyTR_planificacion_de_tareas).
  Los componentes `consola`, `bus_i2c` y la base de `led`, `pulsador` y `bme280`
  proceden de los ejemplos de la asignatura y del Taller 1.
- **Hoja de datos del BME280** de Bosch Sensortec: registros, orden de
  configuración y fórmulas enteras de compensación (apartado 4.2.3), ya usadas
  en el Taller 1.
- **Documentación de ESP-IDF v6.1**: LEDC, `esp_timer`, temporizadores
  software de FreeRTOS y watchdog de tareas.
- **Herramienta de asistencia: Claude Code (Anthropic).** Se usó para escribir
  el código de los componentes `carga`, `comandos`, `resumen`, `sensor`,
  `temporizadores` e `inspeccion`, para reescribir `led` y ampliar `pulsador` y
  `bme280`, para los scripts de captura de la salida serie y para redactar esta
  memoria y los README de los componentes. Las medidas de `resultados/` son
  capturas reales de la placa. El autor revisa, entiende y responde de todo lo
  entregado.
