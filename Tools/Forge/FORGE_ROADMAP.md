# Forge — Roadmap

> Last updated: 2026-04-30
> Primary home: `Tools/Forge/tool/` inside the SagaEngine repository.
> Future: may graduate to a standalone repository the day a non-SagaEngine
> consumer adopts it; the source layout is already structured for that move.

Forge is a **Cargo-flavoured wrapper around CMake and Conan** for C++
projects. Its job is to turn the everyday three-step dance —
*resolve dependencies → configure CMake → build* — into a single
predictable command, while still exposing the underlying tools when a
power user needs to drop down a level.

Crucially, Forge does **not** hide CMake or Conan. Three layers of
control are always available:

1. **High-level (default).** `forge add sfml@2.6` / `forge install` /
   `forge build`.
2. **Intermediate.** `forge build --config Debug --target MyGame` /
   `forge configure --preset release`.
3. **Low-level.** `forge run cmake --build build --target MyGame` /
   `forge run conan install . --build=missing`.

Forge is not an orchestrator and not a dispatcher. It does not know
about Prism, the `tools` dispatcher, or any other tool — it spawns
`cmake` and `conan`, and (in escape-hatch mode) any executable the user
names. Anything else is out of scope.

Conventions: `[x]` shipped and committed, `[ ]` open.
When an item ships, the note after it names the files that represent the work.

---

## 1. Cargo-flavoured CLI

| Status | Item |
|--------|------|
| [x] | `forge --help`, `forge --version`. → `src/main.cpp` |
| [x] | `forge new <dir>` — scaffolds `<dir>/forge.toml`, `<dir>/src/main.cpp`, sane defaults. → `src/main.cpp` |
| [x] | `forge init` — writes `forge.toml` in the current directory. → `src/main.cpp` |
| [x] | `forge add <pkg>[@<ver>]` — appends / updates a `[deps]` entry. → `src/main.cpp` |
| [x] | `forge install` — reads `[deps]`, calls `conan install . --requires=… --build=missing`. → `src/main.cpp` |
| [x] | `forge configure [--source=DIR] [--build=DIR] [--preset=NAME]`. → `src/main.cpp` |
| [x] | `forge build [--build=DIR] [--target=NAME] [--config=Release]`. → `src/main.cpp` |
| [x] | `forge clean [--build=DIR]`. → `src/main.cpp` |
| [x] | `forge run <executable> [args ...]` — escape hatch to any binary on PATH. → `src/main.cpp` |
| [x] | `--strict` flag accepted by every build/install command (currently logs only; enforcement lands with the toolchain item below). → `src/main.cpp` |
| [ ] | `forge test [--label=LABEL]` — wraps `ctest`. |
| [ ] | `forge install-target [--prefix=DIR]` — wraps `cmake --install`. |
| [ ] | `forge presets` — `cmake --list-presets` pretty-printed. |
| [ ] | `forge env` — JSON dump of detected `cmake`/`conan`/compiler versions for scripting. |
| [ ] | `forge bench` — pluggable benchmark runner that respects the manifest's preset. |
| [ ] | `forge fmt` — runs `clang-format` over project sources using the project's `.clang-format`. |

---

## 2. Manifest (`forge.toml`)

The manifest is intentionally tiny. It is **not** a build language — it
is a *build environment specification*. Anything that would belong in
CMake belongs in CMake.

| Status | Item |
|--------|------|
| [x] | Minimal TOML-subset parser — sections + `key = "value"` + line comments. → `include/Forge/Manifest.h`, `src/Manifest.cpp` |
| [x] | Schema sections: `[project]`, `[toolchain]`, `[build]`, `[deps]`. → `src/Manifest.cpp` |
| [x] | Round-trip serialisation that preserves dep order. → `src/Manifest.cpp` |
| [x] | `[deps]` rewriting via `forge add` is in-place when the entry already exists. → `src/Manifest.cpp` |
| [ ] | Real TOML parser conformance — arrays of strings, inline tables, nested keys. Currently anything beyond key/value/section is silently dropped. |
| [ ] | `forge.lock` — locked dependency graph emitted after `forge install`; consumed by future `--strict`. |
| [ ] | `forge.toml` schema documentation in `tool/SCHEMA.md`. |
| [ ] | Validation pass — refuse manifests with unknown top-level sections in `--strict`; warn but accept in flexible mode. |

---

## 3. Reproducibility / Toolchain Pinning

The hardest real-world build problem: Forge + CMake + Conan +
compiler drift breaks builds for everyone downstream. The fix is a
**deterministic execution layer** — Forge is not a new build system,
it just enforces the existing one's inputs.

| Status | Item |
|--------|------|
| [x] | `[toolchain]` section in `forge.toml` (cmake / conan / compiler version pins) is parsed and logged on every build/install. → `src/main.cpp` |
| [ ] | **Strict mode enforcement.** `forge build --strict` (and `install --strict`) verifies that the on-PATH `cmake --version`, `conan --version`, and resolved compiler match the pinned versions. Mismatch → exit 3 (`kExitStrict`). |
| [ ] | `.forge` env-overrides file — small key/value file (`CMAKE=/usr/bin/cmake-3.28 …`) that pins the *binaries*, not just the version strings. Generated alongside `forge.lock`. |
| [ ] | Generated CMake toolchain file — Forge writes `<build>/forge-toolchain.cmake` (pins compiler, std, sysroot) and passes `-DCMAKE_TOOLCHAIN_FILE=…`. |
| [ ] | `conan.lock` integration — `forge install` writes a lockfile by default; `forge install --strict --frozen` refuses without one. |
| [ ] | Auto-switching toolchain — when version mismatch is detected and a versioned binary is present (e.g. `cmake-3.28`), use it instead of the bare `cmake` on PATH. Off by default; opt-in via `[toolchain].auto_switch = true`. |
| [ ] | Two-mode contract — `--strict` (CI / production) refuses mismatches; default (dev) warns but proceeds. Documented in `tool/README.md`. |

---

## 4. Bootstrap Installer (`build.py`)

Forge ships with its own self-contained installer so that
`tools install forge` (the SagaTools dispatcher) and any user with
just Python on PATH can produce a working binary.

| Status | Item |
|--------|------|
| [x] | `tool/build.py` — Python-3 stdlib script: detects `cmake`, configures + builds Forge, stages `bin/forge[.exe]`, runs a smoke test. → `tool/build.py` |
| [x] | `--debug`, `--clean`, `--jobs` flags in the bootstrap. → `tool/build.py` |
| [x] | Multi-config generator support (Release / Debug subdirectories on MSBuild and Xcode). → `tool/build.py` |
| [x] | Installer is idempotent — a re-run rebuilds only what cmake decides has changed. → `tool/build.py` |
| [ ] | Pre-built binary mode — when run as `build.py --download`, the installer fetches a signed release tarball instead of building. |
| [ ] | Hash verification of downloaded binaries against a checksum manifest. |
| [ ] | `build.py --install-to <dir>` — stage to a custom location instead of `bin/`. |
| [ ] | Windows-friendly `build.cmd` shim that just execs `python build.py %*`. |

---

## 5. Loose-Coupling Contract (binding)

Anything that violates these rules invalidates the standalone-tool
guarantee and blocks merge.

| Status | Item |
|--------|------|
| [x] | Forge does NOT include any header from `SagaEngine/`, `SagaEditor/`, `SagaServer/`, `SagaPrism/`, `SagaTools/`, or any other tool. |
| [x] | Forge does NOT walk up to find `SagaEngineRoot.marker`. |
| [x] | Forge does NOT link against any third-party library. |
| [x] | Forge does NOT read any tool-registry JSON. The `tools` dispatcher owns that role. |
| [x] | Forge spawns `cmake` and `conan` for declared subcommands and arbitrary executables only via the explicit `forge run` escape hatch. |
| [x] | Forge does NOT expose a public C++ library; only the `forge` executable is built. |
| [ ] | `Tools/Scripts/check_tools_isolation.py` learns to grep `Tools/Forge/tool/src/**` for forbidden includes and fail CI on hit. |

---

## 6. Build System and Distribution

| Status | Item |
|--------|------|
| [x] | Standalone `tool/CMakeLists.txt` — `cmake_minimum_required(VERSION 3.22)`, `project(forge)`, no `find_package` calls, no Conan, no marker file. → `tool/CMakeLists.txt` |
| [x] | `install(TARGETS forge)`. → `tool/CMakeLists.txt` |
| [x] | Strict warnings: `/W4 /permissive-` (MSVC), `-Wall -Wextra -Wpedantic` (GCC/Clang). → `tool/CMakeLists.txt` |
| [ ] | CI workflow — matrix build across Linux (GCC, Clang) and Windows (MSVC, clang-cl); run `forge --help`, `forge --version`, `forge new fixture && cd fixture && forge configure && forge build`. |
| [ ] | Pre-built signed binaries for Linux x86_64, Linux arm64, Windows x86_64, macOS arm64 attached to tagged releases. |
| [ ] | Single-file release tarball — just the `forge` binary plus `README.md` and `build.py`. |

---

## 7. Engine-Build Decoupling (housekeeping)

The folder `Tools/Forge/cmake/modules/` (a level above this tool) holds
SagaEngine-specific CMake modules consumed by the legacy build scripts
`Tools/Forge/build.ps1` and `Tools/Forge/bootstrap.ps1`. Those modules
were authored for the engine build root that lives in
`Tools/Forge/CMakeLists.txt`. **They are not part of standalone Forge**
and must move out before Forge can publish its own 1.0.

The presence of `Tools/Forge/cmake/modules/Saga*.cmake` (with names
like `SagaCompilerFlags.cmake`, `SagaTargets.cmake`, `SagaTests.cmake`)
is direct proof that this layer is engine-coupled — the standalone
Forge has no business with target naming or test wiring for any
specific engine.

| Status | Item |
|--------|------|
| [ ] | **Rewrite the engine-build orchestration in Python.** `Tools/Forge/build.ps1` (≈37 KB) and `Tools/Forge/bootstrap.ps1` are PowerShell-only and engine-aware; replace them with a Python script (target name TBD, but ergonomically `engine/build.py` at the repo root) that calls Conan and CMake. PowerShell stays only as a thin shim that execs the Python equivalent. |
| [ ] | Relocate `Tools/Forge/cmake/modules/Saga*.cmake` to `cmake/` at the engine root (or `Engine/cmake/`); update the new Python orchestrator to reference the new path. |
| [ ] | Move `Tools/Forge/conanfile.py`, `Tools/Forge/profiles/`, `Tools/Forge/conan.lock` to the engine's build infrastructure home; they describe how the engine consumes Conan, not how Forge itself ships. |
| [ ] | Decide the fate of `Tools/Forge/CMakeLists.txt` (the engine-build root): rename + move, or replace with an explicit thin shim that calls `forge configure` once Forge is the canonical entry. |
| [ ] | Once the above land, `Tools/Forge/` contains only the standalone tool subtree (`tool/`, `FORGE_ROADMAP.md`, `README.md`). The "Forge" name no longer overloads two unrelated things. |
| [ ] | `Tools/Scripts/check_tools_isolation.py` learns to flag any `Saga*.cmake` file under `Tools/Forge/` — once the move is complete, this lint becomes the standing guarantee that engine modules cannot leak back in. |

---

## 8. Standalone-Repo Extraction

| Status | Item |
|--------|------|
| [x] | Tool source lives under `Tools/Forge/tool/`; the contents become the new repo root with zero rewrites. → `tool/README.md` |
| [x] | `tool/README.md` is written for an audience that has never heard of SagaEngine. → `tool/README.md` |
| [x] | No SagaEngine-specific paths or marker references in any header, source, CMake, or comment under `tool/`. → entire `tool/` subtree |
| [ ] | `tool/LICENSE` — placeholder until the licensing decision is made. |
| [ ] | `tool/.github/workflows/ci.yml` — a CI definition that works in a fresh repo with no SagaEngine context. |
| [ ] | `tool/CHANGELOG.md` populated retroactively when the first release is tagged. |
| [ ] | Repo-split rehearsal — a one-shot script under `Tools/Scripts/` that copies `Tools/Forge/tool/` into a temporary directory, runs `build.py`, and reports success. |

---

## 9. Future Scope (Post-1.0)

Items deferred until the core is stable. Anything in this list that
turns Forge into an orchestrator across other tools is automatically
out of scope.

| Status | Item |
|--------|------|
| [ ] | `forge watch` — re-run `cmake --build` on filesystem changes (filesystem watch only; no integration with other tools). |
| [ ] | `forge graph` — render the CMake target graph (`cmake --graphviz`) as PNG / SVG. |
| [ ] | Cache fingerprinting — record configure-stage flags; refuse to re-configure with identical inputs unless `--force`. |
| [ ] | Friendlier error mapping — translate the most common `cmake` failure messages into one-line diagnostics. |
| [ ] | Conan profile management — `forge profile use <name>` writes the right `~/.conan2/profiles/...`. |
| [ ] | Cross-compilation profile presets (`forge target add android-arm64`). |

---

## Definitions

- **Shipped**: implemented, code-reviewed, and present in the current
  repository state. Marked `[x]`.
- **Open**: planned and scoped but not yet implemented. Marked `[ ]`.
- **Post-1.0**: explicitly deferred; will not block a 1.0 tag.

When an item ships, replace `[ ]` with `[x]` and append a note naming the
files that represent the work, matching the convention used in the other
SagaEngine roadmap documents.
