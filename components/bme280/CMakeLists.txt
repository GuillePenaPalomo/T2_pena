# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# La cabecera publica incluye driver/i2c_master.h: esp_driver_i2c va en REQUIRES.
# esp_timer, el reloj con el que se acota la espera de la conversion, solo lo
# usa el codigo: va en PRIV_REQUIRES.
idf_component_register(SRCS "bme280.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_i2c
                       PRIV_REQUIRES esp_timer)
