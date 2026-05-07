cmake_minimum_required(VERSION 3.16)
project(foolscap LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)

add_executable(foolscap-sdl src/ports/sdl/main_sdl.c)

target_link_libraries(foolscap-sdl PRIVATE SDL2::SDL2)

target_compile_options(foolscap-sdl PRIVATE -Wall -Wextra -Wpedantic)
