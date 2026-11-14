# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# La cabecera publica solo usa esp_err.h, de esp_common, que se incluye siempre.
# PRIV_REQUIRES lleva los componentes del proyecto a los que llaman los
# comandos: cada practica anade el suyo.
idf_component_register(SRCS "comandos.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES consola carga sensor led pulsador temporizadores
                                     inspeccion)
