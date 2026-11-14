# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# La cabecera publica usa los tipos de carga, resumen y FreeRTOS: van en
# REQUIRES. esp_timer, la alarma y el reloj en microsegundos, solo lo usa el
# codigo: va en PRIV_REQUIRES.
idf_component_register(SRCS "temporizadores.c"
                       INCLUDE_DIRS "include"
                       REQUIRES carga resumen freertos
                       PRIV_REQUIRES esp_timer)
