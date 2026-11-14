# CMakeLists.txt de componente. Registra las fuentes y las cabeceras publicas.
# La cabecera publica solo usa esp_err.h. heap, la memoria libre, solo lo usa
# el codigo: va en PRIV_REQUIRES.
idf_component_register(SRCS "inspeccion.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES heap)
