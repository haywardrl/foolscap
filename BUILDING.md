# Building Foolscap

## Prerequisites

- A C11 compiler (GCC or Clang)
- CMake 3.16 or later
- SDL2 development headers

On Arch:

```sh
sudo pacman -S base-devel cmake sdl2 clang
```

## Desktop simulator

```sh
cmake -B build
cmake --build build
./build/foolscap-sdl
```

## ESP32-S3 firmware

(To be added — see milestone M2.)

## Project Overview

See [README.md](README.md).
