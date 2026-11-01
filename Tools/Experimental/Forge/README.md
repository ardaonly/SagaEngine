# Forge (Experimental)

Forge is an experimental command-line workflow frontend for C++ projects. C++
developers commonly combine separate tools for project setup, dependency
resolution, configuration, and compilation. Forge's long-term purpose is to
offer one approachable interface around those existing tools without replacing
their build systems or package managers.

## Intended direction

Forge is intended to let a project choose or use suitable dependency providers,
including Conan, vcpkg, a Linux distribution's package manager, or pinned
upstream source. It should also create a minimal CMake or Meson project when
starting from an empty directory. For projects that already contain CMake or
Meson files, Forge should use those files as the source of truth and must not
rewrite or regenerate them.

These are design goals, not current capabilities. The current implementation
primarily wraps CMake and Conan. It does not yet provide provider-neutral
dependency resolution, vcpkg integration, distribution package installation,
or a pinned GitHub-source dependency workflow. Forge is not required to build
SagaEngine; the engine's supported Linux workflow uses Conan and CMake directly.

## Try the current implementation

Build Forge from the SagaEngine repository root:

```sh
python3 Tools/Experimental/Forge/build.py
```

The binary is staged in `Tools/Experimental/Forge/bin/`. Small standalone
examples are under `examples/`; their `forge.toml` files are examples of the
current manifest format.

## Documentation

- [`SCHEMA.md`](SCHEMA.md) describes the currently parsed `forge.toml` fields.
- [`CHANGELOG.md`](CHANGELOG.md) records implemented command changes.
- [`examples/`](examples/) contains runnable examples.

Forge is maintained as a separate experimental tool. Its presence does not
make it part of SagaEngine's runtime or required build chain.
