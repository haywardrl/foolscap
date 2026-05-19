cmake_minimum_required(VERSION 3.16)
project(foolscap LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)

add_compile_options(-Wall -Wextra -Werror -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition)

add_executable(foolscap-sdl
    src/ports/sdl/main_sdl.c
    src/ports/sdl/display_sdl.c
    src/ports/sdl/mem_sdl.c
    src/app/buffer/buffer.c
    src/app/util/utf8.c
    src/app/render/fonts/ibm_plex_mono_20.c
    src/app/render/render.c
    src/app/editor/editor.c
)

target_link_libraries(foolscap-sdl PRIVATE SDL2::SDL2)

target_include_directories(foolscap-sdl PRIVATE src src/app/render/)

enable_testing()
add_subdirectory(tests)
