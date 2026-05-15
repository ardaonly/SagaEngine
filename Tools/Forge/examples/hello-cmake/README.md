# Forge Example: hello-cmake

This is the smallest useful Forge project: one CMake target, no dependencies,
and no generated project logic hidden behind Forge.

## Run

From this directory:

```sh
forge configure
forge build
forge run ./build/hello-cmake
```

To see the backend command without executing it:

```sh
forge configure --explain
forge build --explain
```

Forge coordinates the workflow. CMake still owns the build graph in
`CMakeLists.txt`.
