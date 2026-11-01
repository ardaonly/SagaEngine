# Forge Example: raylib-conan

This example demonstrates the current Conan-backed dependency flow for a small
graphics project. Forge records the dependency in `forge.toml`, asks Conan to
resolve it, then asks the existing CMake project to configure and build.

## Run

From this directory, after `forge` is on `PATH`:

```sh
forge add raylib@5.0
forge install
forge configure
forge build
forge run ./build/raylib-forge-demo
```

The underlying tools remain available directly when you need them:

```sh
conan install . --requires=raylib/5.0 --build=missing
cmake -S . -B build
cmake --build build --target raylib-forge-demo
```

Forge coordinates these tools; it does not replace CMake or Conan.
