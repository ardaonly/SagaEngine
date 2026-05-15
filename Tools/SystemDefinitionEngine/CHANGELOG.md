# Changelog

All notable changes to SDE (System Definition Engine) are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.1] — 2026-05-13

### Added

- `SDE::Core` canonical CMake package target. `SDE::SDE` remains available as a
  pre-1.0 compatibility alias.
- Project-level compiler facade with `SharedRegistrySet`, `CompilerSession`, and
  `CompileContext`.
- Cooperative cancellation token support for compiler integrations.
- Version, dependency manifest, and stable hashing surfaces for schema, data,
  dependency, compiled graph, and artifact identity.
- `EnumRegistry` with schema-loaded enum declarations and enum membership
  validation.
- Structured diagnostic categories, source ranges, deterministic sorting, and
  metadata for editor/CI integration.

### Changed

- CLI validation and compilation now use the public compiler facade.
- `RuleId()` identifies rule behavior, while emitted diagnostic codes remain
  separate machine-readable output.
- Compiled graph instance storage is deterministic.
- Array and object fields are recursively compiled instead of producing null
  placeholders for nested values.
- `nlohmann_json` is now an implementation dependency and is not exposed through
  public headers.

### Verified

- Manual SDE test binary: 76/76 tests passed.
- Manual CLI smoke compile produced deterministic compiled graph and artifact
  hashes.

## [0.1.0] — 2026-05-08

### Added

- **Standalone packaging** — SDE now ships as an independent CMake project with
  its own `conanfile.py`, `version.json`, `CHANGELOG.md`, `LICENSE`, and
  `build.py` bootstrap installer. The directory is repo-extractable: copying
  `Tools/SystemDefinitionEngine/` to a new repository root requires no
  structural rewrite.
- **CMake export config** — installs `SDEConfig.cmake`, `SDEConfigVersion.cmake`,
  and `SDETargets.cmake` so downstream projects can use
  `find_package(SDE CONFIG REQUIRED)` and link against `SDE::Core`.
- **Conan recipe** — `conan create Tools/SystemDefinitionEngine` publishes
  `sde/0.1.0` to the local Conan cache. The recipe declares
  `nlohmann_json/3.11.3` as the only dependency (`gtest/1.14.0` is a
  test-only requirement).
- **Bootstrap installer** — `python3 build.py` orchestrates Conan install +
  CMake configure + build + optional install. `--conan-create` shortcut
  publishes the package; `--no-conan` falls back to a pure CMake flow with
  user-supplied `CMAKE_PREFIX_PATH`.
- **Engine integration** — opt-in `SAGA_WITH_SDE` cache option. Default OFF;
  the engine does not depend on SDE source. When enabled, the engine consumes
  SDE through `find_package(SDE CONFIG REQUIRED)` and the matching Conan
  requirement is added by the engine's `conanfile.py` `with_sde` option.

### Changed

- `add_subdirectory(Tools/SystemDefinitionEngine)` is no longer present in the
  engine root `CMakeLists.txt`. SDE is consumed as a packaged dependency.
- The CLI target is renamed internally to `sde-cli` to avoid colliding with
  the package name in the export set; the produced binary is still `sde`.

### Verified

- `Tools/Scripts/check_tools_isolation.py` confirms no SDE source includes
  `SagaEngine/`, `SagaEditor/`, `SagaServer/`, `SagaPrism/`, or `SagaTools/`,
  and that no `Saga*.cmake` module is referenced from the SDE tree.
