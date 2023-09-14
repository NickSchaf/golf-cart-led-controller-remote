
set(srcF
    "serial_console.c"
)

idf_component_register(SRCS ${srcF} INCLUDE_DIRS "." PRIV_REQUIRES console)
