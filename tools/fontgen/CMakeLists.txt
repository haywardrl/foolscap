cmake_minimum_required(VERSION 3.16)
project(fontgen LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition)

add_executable(fontgen fontgen.c)
target_link_libraries(fontgen PRIVATE m)
