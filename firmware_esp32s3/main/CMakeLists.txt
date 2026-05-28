set(FOOLSCAP_ROOT "${CMAKE_SOURCE_DIR}/..")

idf_component_register(
    SRCS
        "${FOOLSCAP_ROOT}/src/ports/esp32s3/main_esp32s3.c"
        "${FOOLSCAP_ROOT}/src/ports/esp32s3/display_esp32s3.c"
        "${FOOLSCAP_ROOT}/src/ports/esp32s3/mem_esp32s3.c"
        "${FOOLSCAP_ROOT}/src/ports/esp32s3/ble_input_esp32s3.c"
        "${FOOLSCAP_ROOT}/src/ports/esp32s3/esp_hid_gap.c"
        "${FOOLSCAP_ROOT}/src/ports/esp32s3/hid_keymap.c"
        "${FOOLSCAP_ROOT}/src/app/util/utf8.c"
        "${FOOLSCAP_ROOT}/src/app/buffer/buffer.c"
        "${FOOLSCAP_ROOT}/src/app/layout/layout.c"
        "${FOOLSCAP_ROOT}/src/app/editor/editor.c"
        "${FOOLSCAP_ROOT}/src/app/render/render.c"
        "${FOOLSCAP_ROOT}/src/app/render/rect.c"
        "${FOOLSCAP_ROOT}/src/app/render/fonts/ibm_plex_mono_32.c"
    INCLUDE_DIRS
        "${FOOLSCAP_ROOT}/src"
        "${FOOLSCAP_ROOT}/src/app/render"
        "${FOOLSCAP_ROOT}/src/ports/esp32s3"
    REQUIRES
        lilygo_epd47
        bt
        esp_hid
        nvs_flash
        esp_event
)
