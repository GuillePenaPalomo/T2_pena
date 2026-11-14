# Hoja de autoevaluación — Taller 2

Rúbrica del guion (capítulo 10). Propuesta a partir de lo que funciona en el
repositorio y de las capturas de `resultados/`; **revisar y ajustar** con lo que
se enseñó en el turno de demostración.

| Criterio | Puntos (0/1/2) | Por qué |
|---|:---:|---|
| C1 · Tareas y prioridades | 2 | Dos tareas de carga en el núcleo 0; turnos, inanición con el aviso del watchdog (`IDLE0 (CPU 0)`, `CPU 0: carga_a`) y corregida, desde la consola y explicados en el README. |
| C2 · Tarea periódica | 2 | Espera absoluta con `xTaskDelayUntil()`, instante antes de leer, resumen de 50 intervalos sin carga y con carga (jitter 2 y 3 µs), prioridad justificada y extensión con la prioridad invertida (jitter 34 ms). |
| C3 · Salida modulada | 2 | 5 kHz y 13 bits justificados, transición por hardware, una sola tarea maneja LEDC y cada pulsación pasa al modo siguiente desde la tarea, después del antirrebote. |
| C4 · Temporizadores | 2 | Alarma y temporizador software de 10 ms, tarea de servicio en el núcleo 0, jitter sin carga y con carga (0,03 frente a 31 ms) y explicado, periodo y ciclo de trabajo cambiados desde la consola. |
| C5 · Integración y documentación | 2 | Componentes separados, pines solo en `pines.h` y prioridades en `tareas.h`, tabla de tareas con la pila medida, lista de tareas y reparto interpretados, un apartado por práctica, comandos, justificaciones y evidencias. |
| **Total** | **10** | |

**Lo que no llegó a funcionar y por qué.** En el aula solo se llegó a copiar los
componentes y a esbozar las tareas de carga (ver el apartado 12 del README); el
resto se terminó después. La extensión del potenciómetro de P3 no se ha hecho.
En la última captura de P3 el contador marcó 9 pulsaciones cuando se pidieron 8;
no se ha podido confirmar si fue una pulsación de más.
