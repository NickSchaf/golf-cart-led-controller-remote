# idf_component_register(SRCS "cmd_axp192.c" "main.cpp" "cmd_ble.c"
#                     INCLUDE_DIRS ".")

idf_component_register(SRCS "main.cpp" "example_ble_sec_gattc_demo.c" "cmd_ble.c" "cmd_axp192.c"
                       INCLUDE_DIRS "."
                       REQUIRES i2c_manager spi_flash m5core2_axp192 axp192 lvgl lvgl_esp32_drivers nvs_flash bt serial_console cmd_nvs cmd_system)

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_LVGL_H_INCLUDE_SIMPLE")
