# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# La cabecera publica incluye freertos/FreeRTOS.h, por los tipos de prioridad y
# de nucleo: freertos va en REQUIRES. esp_timer, el reloj en microsegundos con
# el que se mide la rafaga, solo lo usa el codigo: va en PRIV_REQUIRES.
idf_component_register(SRCS "carga.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos
                       PRIV_REQUIRES esp_timer)
