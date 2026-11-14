# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# La cabecera publica usa driver/gpio.h y los tipos de FreeRTOS: van en
# REQUIRES. esp_timer, el reloj con el que la rutina de interrupcion marca cada
# flanco, solo lo usa el codigo: va en PRIV_REQUIRES.
idf_component_register(SRCS "pulsador.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio freertos
                       PRIV_REQUIRES esp_timer)
