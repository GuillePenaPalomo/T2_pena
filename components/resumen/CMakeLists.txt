# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# resumen.c es C estandar: no depende de ningun componente de ESP-IDF.
idf_component_register(SRCS "resumen.c"
                       INCLUDE_DIRS "include")
