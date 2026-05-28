# Helper function: defines test executable for individual modules
function(add_unit_test name)
    add_executable(test_${name}
        test_${name}.c
        ${CMAKE_SOURCE_DIR}/tests/unity/src/unity.c
        ${CMAKE_SOURCE_DIR}/src/ports/sdl/mem_sdl.c     # always linked
        ${ARGN}
    )
    target_include_directories(test_${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/unity/src
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_compile_options(test_${name} PRIVATE -fsanitize=address,undefined -g -fno-omit-frame-pointer)
    target_link_options(test_${name} PRIVATE -fsanitize=address,undefined)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

add_unit_test(rect ${CMAKE_SOURCE_DIR}/src/app/render/rect.c)
add_unit_test(hid_keymap ${CMAKE_SOURCE_DIR}/src/ports/esp32s3/hid_keymap.c)
add_unit_test(buffer ${CMAKE_SOURCE_DIR}/src/app/buffer/buffer.c)
add_unit_test(utf8 ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c)
add_unit_test(render
    ${CMAKE_SOURCE_DIR}/src/app/render/render.c
    ${CMAKE_SOURCE_DIR}/src/app/render/rect.c
    ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c
    ${CMAKE_SOURCE_DIR}/src/app/render/fonts/ibm_plex_mono_20.c
)
add_unit_test(find_glyph
    ${CMAKE_SOURCE_DIR}/src/app/render/render.c
    ${CMAKE_SOURCE_DIR}/src/app/render/rect.c
    ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c
)
add_unit_test(put_pixel
    ${CMAKE_SOURCE_DIR}/src/app/render/render.c
    ${CMAKE_SOURCE_DIR}/src/app/render/rect.c
    ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c
)
add_unit_test(draw_glyph
    ${CMAKE_SOURCE_DIR}/src/app/render/render.c
    ${CMAKE_SOURCE_DIR}/src/app/render/rect.c
    ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c
)
add_unit_test(render_fill_rect
    ${CMAKE_SOURCE_DIR}/src/app/render/render.c
    ${CMAKE_SOURCE_DIR}/src/app/render/rect.c
    ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c
)
add_unit_test(layout
    ${CMAKE_SOURCE_DIR}/src/app/layout/layout.c
    ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c
    ${CMAKE_SOURCE_DIR}/src/app/buffer/buffer.c
)
add_unit_test(layout_apply_edit
    ${CMAKE_SOURCE_DIR}/src/app/layout/layout.c
    ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c
    ${CMAKE_SOURCE_DIR}/src/app/buffer/buffer.c
)
add_unit_test(editor
    ${CMAKE_SOURCE_DIR}/src/app/editor/editor.c
    ${CMAKE_SOURCE_DIR}/src/app/buffer/buffer.c
    ${CMAKE_SOURCE_DIR}/src/app/util/utf8.c
    ${CMAKE_SOURCE_DIR}/src/app/render/render.c
    ${CMAKE_SOURCE_DIR}/src/app/render/rect.c
    ${CMAKE_SOURCE_DIR}/src/app/layout/layout.c
    ${CMAKE_SOURCE_DIR}/tests/mocks/display_mock.c
)
