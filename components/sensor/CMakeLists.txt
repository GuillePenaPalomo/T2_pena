# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# La cabecera publica usa los tipos de bme280, carga y resumen: van en REQUIRES.
# esp_timer, el reloj con el que se toma el instante de cada activacion, solo
# lo usa el codigo: va en PRIV_REQUIRES.
idf_component_register(SRCS "sensor.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bme280 carga resumen freertos
                       PRIV_REQUIRES esp_timer)
