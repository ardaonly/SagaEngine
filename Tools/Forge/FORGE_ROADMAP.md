# Forge — Roadmap

> **Primary home:** `Tools/Forge/tool/` inside the SagaEngine repository.
> **Future:** Forge may graduate into a standalone repository once a non-SagaEngine consumer adopts it. The source layout is already structured to make that transition mechanical rather than disruptive.

Forge is a **Cargo-flavoured build front-end for C++**, designed around **CMake and Conan**. Its purpose is to collapse the everyday build sequence — *resolve dependencies → configure CMake → build* — into a predictable, ergonomic command set, while still preserving full access to the underlying tools for advanced users.

Forge is **not** a replacement for CMake, Conan, or Nix. It is a workflow layer with an internal build model that can be lowered into those tools. The user-facing experience should feel simple, but the implementation must remain deep enough to support low-level control, toolchain pinning, reproducibility, and future backend expansion.

Crucially, Forge exposes **three levels of control** at all times:

1. **High-level (default).** `forge add sfml@2.6` / `forge install` / `forge build`.
2. **Intermediate.** `forge build --config Debug --target MyGame` / `forge configure --preset release`.
3. **Low-level.** `forge run cmake --build build --target MyGame` / `forge run conan install . --build=missing`.

Forge is **not** a dispatcher for SagaEngine tools and is **not** coupled to Prism, the `tools` dispatcher, or any other repository-specific orchestration layer. It spawns `cmake` and `conan`, and in escape-hatch mode any executable explicitly named by the user. Anything beyond that is out of scope.

Forge must remain **engine-first but not engine-bound**:

* SagaEngine uses Forge as its preferred build interface.
* Forge itself does not depend on SagaEngine internals.
* When the engine-specific build logic is eventually removed from `Tools/Forge/`, the remaining `tool/` subtree should remain valid as a standalone repository with no structural rewrite.

Forge must also avoid becoming a thin wrapper with no depth. It should own an **internal build representation** that sits between the manifest and the backend tools. That internal model is what allows the project to stay meaningful if Conan is replaced, if CMake is supplemented, or if a new backend is introduced later.

Conventions: `[x]` shipped and committed, `[ ]` open.
When an item ships, the note after it names the files that represent the work.

---

## 1. Cargo-Flavoured CLI

| Status | Item                                                                                                                                             |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| [x]    | `forge --help`, `forge --version`. → `src/main.cpp`                                                                                              |
| [x]    | `forge new <dir>` — scaffolds `<dir>/forge.toml`, `<dir>/src/main.cpp`, and sensible defaults. → `src/main.cpp`                                  |
| [x]    | `forge init` — writes `forge.toml` in the current directory. → `src/main.cpp`                                                                    |
| [x]    | `forge add <pkg>[@<ver>]` — appends or updates a `[deps]` entry. → `src/main.cpp`                                                                |
| [x]    | `forge install` — reads `[deps]`, then calls `conan install . --requires=… --build=missing`. → `src/main.cpp`                                    |
| [x]    | `forge configure [--source=DIR] [--build=DIR] [--preset=NAME]`. → `src/main.cpp`                                                                 |
| [x]    | `forge build [--build=DIR] [--target=NAME] [--config=Release]`. → `src/main.cpp`                                                                 |
| [x]    | `forge clean [--build=DIR]`. → `src/main.cpp`                                                                                                    |
| [x]    | `forge run <executable> [args ...]` — escape hatch to any binary on `PATH`. → `src/main.cpp`                                                     |
| [x]    | `--strict` flag accepted by every build/install command (currently logs only; enforcement lands with the toolchain item below). → `src/main.cpp` |
| [ ]    | `forge test [--label=LABEL]` — wraps `ctest`.                                                                                                    |
| [ ]    | `forge install-target [--prefix=DIR]` — wraps `cmake --install`.                                                                                 |
| [ ]    | `forge presets` — pretty-printed `cmake --list-presets`.                                                                                         |
| [ ]    | `forge env` — JSON dump of detected `cmake` / `conan` / compiler versions for scripting.                                                         |
| [ ]    | `forge bench` — pluggable benchmark runner that respects the manifest preset.                                                                    |
| [ ]    | `forge fmt` — runs `clang-format` over project sources using the project’s `.clang-format`.                                                      |

---

## 2. Manifest (`forge.toml`)

The manifest is intentionally small. It is **not** a build language; it is a **build environment specification**. Anything that belongs in CMake stays in CMake. The manifest should describe intent, constraints, and dependency declarations — not replace the backend.

| Status | Item                                                                                                                                                            |
| ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [x]    | Minimal TOML-subset parser — sections plus `key = "value"` plus line comments. → `include/Forge/Manifest.h`, `src/Manifest.cpp`                                 |
| [x]    | Schema sections: `[project]`, `[toolchain]`, `[build]`, `[deps]`. → `src/Manifest.cpp`                                                                          |
| [x]    | Round-trip serialization that preserves dependency order. → `src/Manifest.cpp`                                                                                  |
| [x]    | `[deps]` rewriting via `forge add` is in place when the entry already exists. → `src/Manifest.cpp`                                                              |
| [ ]    | Real TOML parser conformance — arrays of strings, inline tables, nested keys. Anything beyond section / key-value / comment handling is currently out of scope. |
| [ ]    | `forge.lock` — locked dependency graph emitted after `forge install`; consumed by future `--strict`.                                                            |
| [ ]    | `forge.toml` schema documentation in `tool/SCHEMA.md`.                                                                                                          |
| [ ]    | Validation pass — refuse manifests with unknown top-level sections in `--strict`; warn but accept in flexible mode.                                             |

---

## 3. Internal Build Model and Lowering

This is the part that keeps Forge from becoming a surface-level wrapper. The CLI and manifest are the front door; the real engine of the system is an **internal, backend-neutral build model** that can be lowered into concrete tool invocations.

The internal model should represent:

* project metadata,
* targets,
* dependencies,
* toolchain constraints,
* presets,
* build configuration,
* execution steps,
* and backend-specific overrides.

This layer is what makes it possible to keep the UX stable even if the execution backend changes later.

| Status | Item                                                                                                                       |
| ------ | -------------------------------------------------------------------------------------------------------------------------- |
| [ ]    | Define a backend-neutral internal representation for projects, targets, dependencies, toolchain state, and build steps.    |
| [ ]    | Lower `forge.toml` into the internal representation before any backend-specific logic runs.                                |
| [ ]    | Emit concrete execution plans for CMake and Conan from the internal representation rather than directly from CLI handlers. |
| [ ]    | Preserve an escape hatch for raw backend flags and user-supplied commands without collapsing the abstraction.              |
| [ ]    | Add an explain mode (`forge build --explain`) that prints the lowered execution plan before running it.                    |
| [ ]    | Keep backend adapters replaceable so that future backends can be introduced without rewriting the user-facing model.       |
| [ ]    | Add tests that compare the internal plan against expected backend output for representative projects.                      |

---

## 4. Reproducibility and Toolchain Pinning

The hardest real-world build problem is toolchain drift. Forge should not pretend that CMake and Conan alone solve reproducibility. Instead, Forge should enforce a deterministic execution layer around them.

| Status | Item                                                                                                                                                                                                                                    |
| ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [x]    | `[toolchain]` section in `forge.toml` (CMake / Conan / compiler version pins) is parsed and logged on every build/install. → `src/main.cpp`                                                                                             |
| [ ]    | **Strict mode enforcement.** `forge build --strict` and `forge install --strict` verify that `cmake --version`, `conan --version`, and the resolved compiler match the pinned versions. Mismatch exits with `kExitStrict`.              |
| [ ]    | `.forge` env-overrides file — small key-value file (`CMAKE=/usr/bin/cmake-3.28 …`) that pins the actual binaries, not just version strings. Generated alongside `forge.lock`.                                                           |
| [ ]    | Generated CMake toolchain file — Forge writes `<build>/forge-toolchain.cmake` to pin compiler, language standard, and sysroot, then passes `-DCMAKE_TOOLCHAIN_FILE=…`.                                                                  |
| [ ]    | `conan.lock` integration — `forge install` writes a lockfile by default; `forge install --strict --frozen` refuses to proceed without one.                                                                                              |
| [ ]    | Auto-switching toolchain — when a version mismatch is detected and a versioned binary is available on the system (for example `cmake-3.28`), use it instead of the bare `cmake` on `PATH`. Opt-in via `[toolchain].auto_switch = true`. |
| [ ]    | Two-mode contract — `--strict` for CI and production refuses mismatches; default developer mode warns but proceeds. Documented in `tool/README.md`.                                                                                     |

---

## 5. Bootstrap Installer (`build.py`)

Forge ships with its own self-contained installer so that `tools install forge` and any user with only Python on `PATH` can produce a working binary.

| Status | Item                                                                                                                                                        |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [x]    | `tool/build.py` — Python 3 stdlib script: detects `cmake`, configures and builds Forge, stages `bin/forge[.exe]`, then runs a smoke test. → `tool/build.py` |
| [x]    | `--debug`, `--clean`, `--jobs` flags in the bootstrap. → `tool/build.py`                                                                                    |
| [x]    | Multi-config generator support (Release / Debug subdirectories on MSBuild and Xcode). → `tool/build.py`                                                     |
| [x]    | Installer is idempotent — a re-run rebuilds only what CMake determines has changed. → `tool/build.py`                                                       |
| [ ]    | Pre-built binary mode — when run as `build.py --download`, the installer fetches a signed release tarball instead of building.                              |
| [ ]    | Hash verification of downloaded binaries against a checksum manifest.                                                                                       |
| [ ]    | `build.py --install-to <dir>` — stage to a custom location instead of `bin/`.                                                                               |
| [ ]    | Windows-friendly `build.cmd` shim that simply executes `python build.py %*`.                                                                                |

---

## 6. Loose-Coupling Contract

Anything that violates these rules invalidates the standalone-tool guarantee and blocks merge.

| Status | Item                                                                                                                                   |
| ------ | -------------------------------------------------------------------------------------------------------------------------------------- |
| [x]    | Forge does **not** include any header from `SagaEngine/`, `SagaEditor/`, `SagaServer/`, `SagaPrism/`, `SagaTools/`, or any other tool. |
| [x]    | Forge does **not** walk up to find `SagaEngineRoot.marker`.                                                                            |
| [x]    | Forge does **not** link against any third-party library.                                                                               |
| [x]    | Forge does **not** read any tool-registry JSON. The `tools` dispatcher owns that role.                                                 |
| [x]    | Forge spawns `cmake` and `conan` for declared subcommands and arbitrary executables only via the explicit `forge run` escape hatch.    |
| [x]    | Forge does **not** expose a public C++ library; only the `forge` executable is built.                                                  |
| [ ]    | `Tools/Scripts/check_tools_isolation.py` learns to grep `Tools/Forge/tool/src/**` for forbidden includes and fail CI on hit.           |

---

## 7. Build System and Distribution

| Status | Item                                                                                                                                                                                           |
| ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [x]    | Standalone `tool/CMakeLists.txt` — `cmake_minimum_required(VERSION 3.22)`, `project(forge)`, no `find_package` calls, no Conan, no marker file. → `tool/CMakeLists.txt`                        |
| [x]    | `install(TARGETS forge)`. → `tool/CMakeLists.txt`                                                                                                                                              |
| [x]    | Strict warnings: `/W4 /permissive-` for MSVC, `-Wall -Wextra -Wpedantic` for GCC and Clang. → `tool/CMakeLists.txt`                                                                            |
| [ ]    | CI workflow — matrix build across Linux (GCC, Clang) and Windows (MSVC, clang-cl); run `forge --help`, `forge --version`, `forge new fixture && cd fixture && forge configure && forge build`. |
| [ ]    | Pre-built signed binaries for Linux x86_64, Linux arm64, Windows x86_64, and macOS arm64 attached to tagged releases.                                                                          |
| [ ]    | Single-file release tarball — just the `forge` binary plus `README.md` and `build.py`.                                                                                                         |

---

## 8. Engine-Build Decoupling

The folder `Tools/Forge/cmake/modules/` currently contains SagaEngine-specific CMake modules consumed by the legacy build scripts `Tools/Forge/build.ps1` and `Tools/Forge/bootstrap.ps1`. Those modules were authored for the engine build root that lives in `Tools/Forge/CMakeLists.txt`. They are **not** part of standalone Forge and must be moved out before Forge can reach 1.0 as an independent tool.

The presence of `Tools/Forge/cmake/modules/Saga*.cmake` — such as `SagaCompilerFlags.cmake`, `SagaTargets.cmake`, and `SagaTests.cmake` — is direct evidence that this layer is engine-coupled. Standalone Forge has no business knowing about engine-specific target naming, test wiring, or repository-specific conventions.

| Status | Item                                                                                                                                                                                                                                                                                                     |
| ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [ ]    | Rewrite the engine-build orchestration in Python. `Tools/Forge/build.ps1` and `Tools/Forge/bootstrap.ps1` are PowerShell-only and engine-aware; replace them with a Python script at the repository root, ideally `engine/build.py`, that calls Conan and CMake. PowerShell remains only as a thin shim. |
| [ ]    | Relocate `Tools/Forge/cmake/modules/Saga*.cmake` to `cmake/` at the engine root (or `Engine/cmake/`) and update the new Python orchestrator to reference the new path.                                                                                                                                   |
| [ ]    | Move `Tools/Forge/conanfile.py`, `Tools/Forge/profiles/`, and `Tools/Forge/conan.lock` to the engine build infrastructure home; they describe how the engine consumes Conan, not how Forge ships.                                                                                                        |
| [ ]    | Decide the fate of `Tools/Forge/CMakeLists.txt` as the engine-build root: rename and move it, or replace it with a thin shim that invokes `forge configure` once Forge becomes the canonical entry.                                                                                                      |
| [ ]    | Once the above are complete, `Tools/Forge/` should contain only the standalone tool subtree (`tool/`, `FORGE_ROADMAP.md`, `README.md`). The Forge name no longer overlaps two unrelated responsibilities.                                                                                                |
| [ ]    | `Tools/Scripts/check_tools_isolation.py` learns to flag any `Saga*.cmake` file under `Tools/Forge/`; after the move, this becomes the standing guarantee that engine modules cannot leak back in.                                                                                                        |

---

## 9. Standalone-Repo Extraction

| Status | Item                                                                                                                                                              |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [x]    | Tool source lives under `Tools/Forge/tool/`; the contents can become a new repository root with zero rewrites. → `tool/README.md`                                 |
| [x]    | `tool/README.md` is written for an audience that has never heard of SagaEngine. → `tool/README.md`                                                                |
| [x]    | No SagaEngine-specific paths or marker references in any header, source, CMake file, or comment under `tool/`. → entire `tool/` subtree                           |
| [ ]    | `tool/LICENSE` — placeholder until the licensing decision is made.                                                                                                |
| [ ]    | `tool/.github/workflows/ci.yml` — a CI definition that works in a fresh repository with no SagaEngine context.                                                    |
| [ ]    | `tool/CHANGELOG.md` populated retroactively when the first release is tagged.                                                                                     |
| [ ]    | Repo-split rehearsal — a one-shot script under `Tools/Scripts/` that copies `Tools/Forge/tool/` into a temporary directory, runs `build.py`, and reports success. |

---

## 10. NixOS Configuration Layer

Forge must remain compatible with NixOS without turning into a package manager or a second source of truth for the toolchain. The role of this section is to ensure that Forge behaves correctly in a Nix-based environment while avoiding accidental overlap with Nix, Conan, or CMake responsibilities.

Forge should assume that the user may already provide compilers, SDKs, and libraries such as SDL, Vulkan, GLFW, or raylib through Nix. In that case, Forge must not attempt to install, replace, or re-resolve those dependencies through Conan unless explicitly instructed. Its job is to respect the active Nix environment, keep the build process deterministic, and avoid duplicate dependency sources at configure or link time.

| Status | Item                                                                                                                                                                                                        |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [ ]    | **NixOS environment detection.** Forge detects when it is running inside NixOS or a Nix development shell and adjusts its execution model accordingly.                                                      |
| [ ]    | **Nix-aware build mode.** In Nix mode, Forge does not assume globally installed system tools and does not try to manage the host toolchain itself.                                                          |
| [ ]    | **Dependency source precedence.** When a dependency is provided by Nix, Forge treats that dependency as authoritative and avoids re-adding it through Conan unless explicitly requested.                    |
| [ ]    | **No duplicate resolution.** Forge detects when libraries such as SDL, Vulkan, GLFW, or raylib are already supplied by the Nix environment and avoids resolving them again through Conan.                   |
| [ ]    | **Toolchain isolation.** Forge generates build commands that respect the active Nix compiler, sysroot, and linker environment without leaking host-system assumptions into the build.                       |
| [ ]    | **Optional Nix shell integration.** Forge may offer an opt-in path for launching builds through `nix develop`, but it must not require Nix-specific bootstrap logic to function.                            |
| [ ]    | **Deterministic environment contract.** A project built under the same `forge.toml` and the same Nix environment should produce the same build behavior across machines.                                    |
| [ ]    | **Strict separation of responsibilities.** Nix provides the environment, Conan provides dependency resolution when needed, CMake performs configuration and generation, and Forge coordinates the workflow. |

The purpose of this section is not to make Forge depend on Nix. The purpose is to make Forge behave correctly when Nix is already part of the environment. This prevents toolchain drift, duplicate dependency resolution, and accidental conflicts between Nix-managed packages and Conan-managed packages.

In practical terms, this section exists so that a user can keep using NixOS for system-level reproducibility while Forge continues to provide a simple Cargo-like build interface on top of CMake and Conan.

---

## 11. Future Scope (Post-1.0)

Items deferred until the core is stable. Anything in this list that turns Forge into an orchestrator across other tools is automatically out of scope.

| Status | Item                                                                                                                  |
| ------ | --------------------------------------------------------------------------------------------------------------------- |
| [ ]    | `forge watch` — re-run `cmake --build` on filesystem changes; filesystem watch only, no integration with other tools. |
| [ ]    | `forge graph` — render the CMake target graph (`cmake --graphviz`) as PNG or SVG.                                     |
| [ ]    | Cache fingerprinting — record configure-stage flags; refuse to re-configure with identical inputs unless `--force`.   |
| [ ]    | Friendlier error mapping — translate the most common CMake failure messages into one-line diagnostics.                |
| [ ]    | Conan profile management — `forge profile use <name>` writes the right `~/.conan2/profiles/...`.                      |
| [ ]    | Cross-compilation profile presets (`forge target add android-arm64`).                                                 |

---

## Definitions

* **Shipped**: implemented, code-reviewed, and present in the current repository state. Marked `[x]`.
* **Open**: planned and scoped but not yet implemented. Marked `[ ]`.
* **Post-1.0**: explicitly deferred; will not block a 1.0 tag.

When an item ships, replace `[ ]` with `[x]` and append a note naming the files that represent the work, matching the convention used in the other SagaEngine roadmap documents.