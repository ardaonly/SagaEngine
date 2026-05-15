# forge.toml — Schema Reference

`forge.toml` is the project manifest. It describes *project identity*, *toolchain constraints*, and *dependency declarations*. It is not a build language; anything that belongs in CMake stays in CMake.

## Syntax

A strict subset of TOML:

- Section headers: `[section]`
- Key-value pairs: `key = "value"` or `key = 'value'` or `key = bare-token`
- Line comments: `# comment`
- Arrays, inline tables, and multi-line values are not supported.
- Unknown sections are preserved on save and reported as errors in `--strict` mode.

## Sections

### `[project]`

| Key       | Type   | Description                                                   |
|-----------|--------|---------------------------------------------------------------|
| `name`    | string | Project name (used for display and scaffolding).              |
| `version` | string | Project version string (advisory; not consumed by Forge).     |

### `[toolchain]`

Version pins verified by `--strict` mode. Forge checks that the tools on `PATH` satisfy these constraints before running any build or install step.

| Key        | Type   | Description                                                                       |
|------------|--------|-----------------------------------------------------------------------------------|
| `cmake`    | string | Required CMake version prefix. `"3.28"` matches `3.28.0`, `3.28.1`, not `3.29.0`. |
| `conan`    | string | Required Conan version prefix. `"2.0"` matches `2.0.x`, not `2.1.x`.              |
| `compiler` | string | Required compiler string matched against `c++ --version` output.                  |

Pin matching uses a prefix rule: pin `"3.28"` satisfies `3.28.0` and `3.28.1` but not `3.29.0`.

### `[build]`

| Key       | Type   | Default   | Description                                                          |
|-----------|--------|-----------|----------------------------------------------------------------------|
| `backend` | string | `"cmake"` | Build backend. Only `"cmake"` is implemented.                        |
| `preset`  | string | —         | CMake preset name passed to `cmake --preset`.                        |
| `source`  | string | `"."`     | CMake source directory (`-S`). Ignored when `preset` is set.         |
| `build`   | string | `"build"` | CMake binary directory (`-B` / `cmake --build`). Ignored when `preset` is set. |

When `preset` is set, `source` and `build` are overridden by the preset's own `binaryDir` for the configure step.

### `[deps]`

Ordered dependency declarations consumed by `forge install` when no `--profile` is given and no `conanfile.py` is present.

```toml
[deps]
sfml  = "2.6"
boost = "1.83"
```

Each key is a Conan package name; each value is the version string passed as `--requires=name/version`. Dependencies are installed in declaration order.

When a `conanfile.py` exists in the working directory, `[deps]` is optional — `forge install` falls back to the conanfile automatically (mode C).

## Install dispatch modes

`forge install` selects a mode in this priority order:

| # | Condition | Conan invocation |
|---|-----------|-----------------|
| A | `--profile <name>` given | `conan install . --profile:host=<name> [--profile:build=<name>] --build=missing` |
| B | `[deps]` section present | `conan install . --requires=pkg/ver ... --build=missing` |
| C | `conanfile.py` exists | `conan install . --build=missing` |
| — | None of the above | Error; nothing to install |

## forge.lock

After a successful `forge install`, Forge writes `forge.lock` in the working directory. It records the install mode and declared dependencies. Commit it alongside `forge.toml` for reproducible builds. It is consumed by `--strict --frozen` (planned).

Example `forge.lock` for mode A:

```json
{
  "forge_lock_version": 1,
  "mode": "profile",
  "profile_host": "windows-msvc",
  "profile_build": "windows-build",
  "deps": []
}
```

## .forge env-overrides

A `.forge` file in the working directory overrides the executable names used by adapters. Forge loads it at startup if present.

```ini
# .forge — pin specific binary paths
CMAKE        = /usr/bin/cmake-3.28
CONAN        = /home/user/.local/bin/conan
CTEST        = /usr/bin/ctest
CLANG_FORMAT = /usr/bin/clang-format-17
```

## Full example

```toml
[project]
name    = "mygame"
version = "0.1.0"

[toolchain]
cmake    = "3.28"
conan    = "2.0"
compiler = "clang-17"

[build]
preset = "release"
build  = "build/release"

[deps]
sfml   = "2.6"
spdlog = "1.12"
```
