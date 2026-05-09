# Building Foolscap

## Prerequisites

- A C11 compiler (GCC or Clang)
- CMake 3.16 or later
- SDL2 development headers

On Arch:

```sh
sudo pacman -S base-devel cmake sdl2 clang
```

## Cloning

This project uses git submodules for test dependencies. After cloning, run:

```sh
git submodule update --init --recursive
```

This fetches Unity (the test framework) at the version this project uses.

## Desktop simulator

```sh
cmake -B build && cmake --build build
./build/foolscap-sdl
```

## ESP32-S3 firmware

To be added

## Unity Tests

```sh
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R buffer    # run only tests matching "buffer"
```

## Editor setup

The project assumes an LSP-driven editor (Neovim, Emacs, etc). `compile_commands.json` is generated automatically by CMake; point your LSP (`clangd`) at it.

For Neovim: ensure `compile_commands.json` is symlinked or copied to repo root after building, or configure clangd's `--compile-commands-dir=build`.

For Emacs: `lsp-mode` or `eglot` with `clangd` backend works out of the box.

## Maintainer Notes

```sh
git submodule add https://github.com/ThrowTheSwitch/Unity.git tests/unity
cd tests/unity
git fetch --tags
git checkout v2.6.0
cd ../..
git submodule status
```

Checkout a different tag inside tests/unity to upgrade/downgrade unity test framework version.


## Project Overview

See [README.md](README.md).
