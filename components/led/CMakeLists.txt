# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# La cabecera publica usa gpio_num_t, de driver/gpio.h, y los tipos de
# FreeRTOS: esp_driver_gpio y freertos van en REQUIRES. esp_driver_ledc, la
# salida modulada, solo la usa el codigo: va en PRIV_REQUIRES.
idf_component_register(SRCS "led.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio freertos
                       PRIV_REQUIRES esp_driver_ledc)
