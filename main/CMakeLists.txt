# CMakeLists.txt de componente. Registra las fuentes del programa.
# PRIV_REQUIRES dice de que componentes depende main: los del propio proyecto
# que inicia app_main, y esp_driver_gpio, porque pines.h usa GPIO_NUM_x.
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio bus_i2c bme280 carga comandos
                                     led pulsador sensor temporizadores)
