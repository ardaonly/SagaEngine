# Changelog

All notable changes to Forge are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [0.3.0] — 2026-05-07

### Added

- **Internal build model** (`BuildModel`) — backend-neutral intermediate representation lowered from `forge.toml` before any adapter sees it. CLI flag overrides are applied after lowering.
- **Backend adapter pattern**: `CMakeAdapter` and `ConanAdapter` are the only files that invoke `cmake`/`ctest` and `conan` respectively. No other file in Forge spawns these tools.
- **`ToolEnv`** — process-wide executable resolution table loaded from `.forge` env-overrides at startup. Adapters use `ToolEnv::Active()` instead of hardcoded binary names.
- **`EnvProbe`** — version detection for cmake, conan, and the system C++ compiler (tries `c++`, `g++`, `clang++`). Used by `forge env` and `--strict` enforcement.
- **`forge test`** `[--build=DIR] [--label=LABEL] [--verbose]` — wraps `ctest --test-dir`.
- **`forge install-target`** `[--build=DIR] [--prefix=DIR] [--component=NAME]` — wraps `cmake --install`.
- **`forge presets`** `[build|test|configure]` — wraps `cmake --list-presets`.
- **`forge env`** `[--json]` — prints detected cmake, conan, and compiler versions; optionally as JSON.
- **`forge fmt`** `[--source=DIR] [--explain]` — runs `clang-format -i` on all `.cpp`/`.h`/`.hpp`/`.cxx`/`.cc`/`.hxx` files found recursively under the source dir.
- **`--explain`** flag on `configure`, `build`, `clean`, `install`, and `fmt` — prints the backend command line without executing it.
- **Strict mode enforcement** — `--strict` now verifies that cmake, conan, and compiler versions on `PATH` match the `[toolchain]` pins in `forge.toml` via `EnvProbe::CheckStrict`.
- **Manifest validation** — `--strict` rejects manifests with unknown top-level sections.
- **`forge.lock`** written after successful `forge install` — records install mode (`profile` / `manifest` / `conanfile`) and declared dependencies for future `--strict --frozen` verification.
- **`.forge` env-overrides** loaded at startup from the working directory if present.
- **`forge new`** now scaffolds a minimal `CMakeLists.txt` alongside `forge.toml` and `src/main.cpp`.

### Changed

- `ConanAdapter::Install` uses `ToolEnv::Active().conan` instead of the hardcoded string `"conan"`.
- `CMakeAdapter` methods use `ToolEnv::Active().cmake` / `.ctest` instead of hardcoded strings.
- `CmdNew` no longer sets a `preset` key in the scaffolded `forge.toml` — a bare project should not assume preset availability.

---

## [0.2.0]

### Added

- `forge install --profile <name> [--profile-build <name>]` — Conan profile-aware install.
- `ConanAdapter` — three dispatch modes: host profile, manifest `[deps]`, and `conanfile.py` fallback.
- `forge.toml` `[build]` section consumed by configure/build commands.
- `forge run <exe>` escape hatch.

---

## [0.1.0]

### Added

- `forge new`, `forge init`, `forge add`.
- `forge install`, `forge configure`, `forge build`, `forge clean`.
- Minimal TOML-subset parser (`Manifest`).
- `build.py` bootstrap installer with `--debug`, `--clean`, `--jobs`.
- Strict warning flags: `/W4 /permissive-` (MSVC), `-Wall -Wextra -Wpedantic` (GCC/Clang).
