/* tareas.h - las tareas del proyecto, en un solo sitio.
 *
 * Como los pines en pines.h: la prioridad, el núcleo y la pila de cada tarea
 * propia se escriben aquí y en ningún otro fichero. Los componentes los
 * reciben al iniciarse, y la tabla de tareas del README tiene que decir lo
 * mismo que este fichero. Verlas juntas es lo que permite razonar sobre ellas:
 * una prioridad no significa nada sola, sino frente a las demás.
 *
 * Las prioridades de FreeRTOS que ya están puestas y sirven de referencia:
 *   0   tarea inactiva de cada núcleo (IDLE0, IDLE1)
 *   1   tarea de servicio de temporizadores (Tmr Svc) y app_main
 *   5   tarea de la consola, que la fija su componente, sin afinidad
 *   22  tarea de esp_timer, en el núcleo 0
 */
#pragma once

/* El núcleo 0 es el de la prueba: en él van la carga y todo lo que la carga
 * tiene que poder retrasar. En el núcleo 1 van las tareas que observan, para
 * que observar no altere lo que se observa y sigan funcionando aunque el
 * núcleo 0 quede acaparado. */
#define NUCLEO_PRUEBA 0
#define NUCLEO_OBSERVACION 1

/* Tareas de carga (P1). Por encima de 1, para que retrasen a la tarea de
 * servicio de temporizadores, que es lo que mide P4; por debajo de 5, para que
 * la consola siga respondiendo aunque una acapare el núcleo. En inanición y
 * corregida, «carga_a» sube un escalón: lo justo para pasar delante de
 * «carga_b». */
#define PRIORIDAD_CARGA 2
#define PRIORIDAD_CARGA_ALTA 3
#define PILA_CARGA 3072

/* Tarea del informe de la carga (P1). Solo escribe una línea por segundo: su
 * prioridad importa poco, y va en el núcleo de observación. */
#define PRIORIDAD_INFORME 5
#define PILA_INFORME 3072
