# Forge Example: raylib-conan

This example shows Forge's intended dependency workflow for a small graphics
project. Forge records the dependency in `forge.toml`, asks Conan to resolve it,
then asks CMake to configure and build the project.

## Run

From this directory, after `forge` is on `PATH`:

```sh
forge add raylib@5.0
forge install
forge configure
forge build
forge run ./build/raylib-forge-demo
```

The equivalent low-level backend calls remain available when you need them:

```sh
forge run conan install . --requires=raylib/5.0 --build=missing
forge run cmake -S . -B build
forge run cmake --build build --target raylib-forge-demo
```

Forge should reduce the everyday command sequence, not hide CMake or Conan from
the project.
