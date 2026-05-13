# Helper function: defines test executable for individual modules
function(add_unit_test name)
    add_executable(test_${name}
        test_${name}.c
        ${CMAKE_SOURCE_DIR}/tests/unity/src/unity.c
        # any extra source files passed in
        ${ARGN}
    )
    target_include_directories(test_${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/unity/src
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_options(test_${name} PRIVATE -fsanitize=address,undefined -g -fno-omit-frame-pointer)
    target_link_options(test_${name} PRIVATE -fsanitize=address,undefined)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

add_unit_test(buffer ${CMAKE_SOURCE_DIR}/src/app/buffer/buffer.c)
add_unit_test(render ${CMAKE_SOURCE_DIR}/src/app/render/font.h)
