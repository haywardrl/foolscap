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
cmake -B build && cmake --build build
./build/foolscap-sdl
```

## ESP32-S3 firmware

To be added

## Editor setup

The project assumes an LSP-driven editor (Neovim, Emacs, etc). `compile_commands.json` is generated automatically by CMake; point your LSP (`clangd`) at it.

For Neovim: ensure `compile_commands.json` is symlinked or copied to repo root after building, or configure clangd's `--compile-commands-dir=build`.

For Emacs: `lsp-mode` or `eglot` with `clangd` backend works out of the box.

## Project Overview

See [README.md](README.md).
