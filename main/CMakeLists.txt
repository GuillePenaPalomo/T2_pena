# CMakeLists.txt de componente. Registra las fuentes del programa.
# PRIV_REQUIRES dice de que componentes depende main: los del propio proyecto
# que inicia app_main.
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES carga comandos)
