/* comandos.h - los comandos de la consola del Taller 2.
 *
 * El componente consola no sabe qué comandos hay: recibe una tabla. Esta es la
 * tabla del proyecto, con un comando por cada cosa que las prácticas dejan
 * cambiar o consultar sin volver a compilar. Cada práctica añade sus filas.
 *
 * Los comandos no hacen el trabajo: llaman a la interfaz pública del
 * componente que corresponde.
 */
#pragma once

#include "esp_err.h"

/* Arranca la consola en la UART del monitor con la tabla del proyecto. Se
 * llama al final, cuando los demás componentes ya están iniciados. */
esp_err_t comandos_iniciar(void);
