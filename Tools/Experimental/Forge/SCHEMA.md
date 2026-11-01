# `forge.toml` Reference

This page describes the small manifest format understood by the current
experimental Forge implementation. It is not a general TOML implementation or
a promise that every field is consumed by every command.

```toml
[project]
name = "my-cpp-project"

[toolchain]
cmake = "3.28"
conan = "2"

[build]
backend = "cmake"
preset = "release"
source = "."
build = "build"

[deps]
fmt = "10.2.1"
```

All sections are optional. Values are strings. The current parser supports
line comments, quoted or bare scalar values, and a dependency map. It is
intentionally limited; use a full TOML parser for richer manifest needs.

| Section | Current purpose |
| --- | --- |
| `[project]` | Project name used by Forge commands. |
| `[toolchain]` | Optional tool version pins inspected by strict checks. |
| `[build]` | CMake backend, preset, source directory, build directory, and related command settings. |
| `[deps]` | Dependency names and versions consumed by the current Conan workflow. |

Unknown sections are rejected in strict mode. See the examples for the
currently supported command flow. Provider-neutral dependency declarations,
vcpkg, system package managers, and pinned source fetching remain future goals.
