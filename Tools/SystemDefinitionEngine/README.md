# System Definition Engine (SDE)

SDE is a standalone model definition, validation, and compilation pipeline. It is
not a serializer, not an editor, and not a runtime component. It is a compiler
stage that validates authored data and produces deterministic, read-only graph
output before runtime systems consume it.

> **Standalone library.** SDE ships as an independent C++ static library
> packaged through Conan. The engine consumes it as an external dependency,
> never as embedded source. The contents of `Tools/SystemDefinitionEngine/`
> can become a standalone repository root with no structural rewrite.
>
> **License.** SDE is Apache-2.0 under Arda Koyuncu. The SagaEngine monorepo is
> the canonical source; standalone repositories are mirrors or release exports.

## What it does

Content authors define game entities (Items, Skills, Recipes, etc.) in JSON files.
SDE parses those files, validates them against declared schemas and business rules,
resolves cross-model references, and produces a compiled, read-only graph that the
editor and runtime consume safely.

If any field type is wrong, any reference is dangling, or any business rule fails,
SDE refuses to publish an artifact and emits structured diagnostics with severity,
category, stable code, source range, and machine-readable metadata. The runtime
never loads malformed data.

## Pipeline

```
File Input
   |
   v
ModelLoader (JSON)
   |
   v
Raw ModelInstances  (RawValue tree, per-node SourceLocation)
   |
   v
Validator           (schema type-check + per-field rules + cross-field Predicates)
   |
   v
ReferenceResolver   (string IDs → CompiledInstanceRef handles, cycle detection)
   |
   v
ModelCompiler       (BuildGraph)
   |
   v
CompiledModelGraph  (read-only, slab-backed, compound-key indexed)
   |
   v
Editor / Runtime consumption
```

Each stage has a single responsibility. Errors are caught at the earliest possible stage.

## Public API surface

Primary consumers should use the project-level facade:

- `SDE/Compiler/CompilerSession.h` — lifecycle-oriented project validation/compile API
- `SDE/Compilation/CompiledModelGraph.h` — immutable, reference-resolved output
- `SDE/Validation/Diagnostic.h` — structured diagnostics

Link against `SDE::Core`; do not include `src/` internals. `SDE::SDE` exists only
as a pre-1.0 compatibility alias.

### Compiler lifecycle

- `SharedRegistrySet` owns type, rule, and enum registries. It is reusable only
  after `Freeze()`.
- `CompilerSession` owns project layout, schema loading, dependency metadata,
  version state, and future cache surfaces.
- `CompileContext` is transient per compile and carries cancellation/fail-fast
  policy.
- Cancellation is cooperative and token-based. Cancelled compiles return a
  structured result and never publish a graph or artifact.
- There are no global mutable registries and no singleton compiler state.

### Versioning and compatibility

SDE tracks separate schema, data, compiler, and artifact format versions. The
current production contract is same-major compatibility, backward-compatible minor
updates, and patch releases that do not change serialized behavior. Unsupported
schema or data versions fail with structured migration diagnostics. SDE is pre-1.0,
so there is no binary ABI guarantee; public headers should avoid layout-sensitive
or allocator-sensitive runtime boundaries where practical.

## Project directory layout

```
my-game-data/
├── schemas/              # One *.json per model kind (ModelDefinition)
│   ├── Item.json
│   └── Skill.json
└── data/                 # Instance data files
    ├── items/
    │   └── weapons.json  # { "modelId": "Item", "data": [...] }
    └── skills/
        └── combat.json
```

### Schema file format

```json
{
  "id": "Item",
  "displayName": "Item",
  "schemaVersion": 1,
  "enums": [
    { "id": "WeaponCategory", "members": [ "melee", "ranged" ] }
  ],
  "fields": [
    { "id": "name",       "type": "Text",    "presence": "required" },
    { "id": "damage_min", "type": "Integer", "presence": "required" },
    { "id": "category",   "type": "Enum<WeaponCategory>", "presence": "required" },
    { "id": "recipe",     "type": "Ref<Recipe>", "presence": "optional" }
  ]
}
```

### Data file format

```json
{
  "modelId": "Item",
  "dataVersion": 1,
  "data": [
    { "id": "sword_01", "name": "Iron Sword", "damage_min": 10, "category": "melee" }
  ]
}
```

### Type strings

| String              | Meaning                                      |
|---------------------|----------------------------------------------|
| `Number`            | IEEE-754 double                              |
| `Integer`           | Signed 64-bit integer                        |
| `Text`              | String                                       |
| `Boolean`           | true / false                                 |
| `Color`             | Color value                                  |
| `Enum<EnumId>`      | Named enum member                            |
| `Ref<ModelId>`      | Reference to an instance of another model    |
| `List<T>`           | Homogeneous ordered sequence                 |
| `Map<K,V>`          | String-keyed map                             |

## CLI usage

```bash
# Validate only (schema + rules; no output file)
sde validate my-game-data/

# Full compile: validate + resolve + emit compiled.json
sde compile my-game-data/

# Custom output path
sde compile my-game-data/ --out=build/data.compiled.json
```

Exit codes: `0` = Clean, `1` = Warnings, `2` = Fail, `3` = Usage error, `4` = I/O error.

## Building

### Bootstrap (recommended)

```bash
python3 Tools/SystemDefinitionEngine/build.py             # Conan + CMake build
python3 Tools/SystemDefinitionEngine/build.py --tests     # also build SDETests
python3 Tools/SystemDefinitionEngine/build.py --conan-create  # publish sde/0.1.1 locally
```

The `bin/sde` CLI is staged automatically. Pass `--install-prefix=<dir>` to
also run `cmake --install`.

### Manual (Conan)

```bash
conan create Tools/SystemDefinitionEngine
```

This publishes `sde/0.1.1` to the local Conan cache. Downstream projects then
declare `self.requires("sde/0.1.1")` and link against `SDE::Core`.

### Manual (plain CMake)

```bash
cmake -S Tools/SystemDefinitionEngine -B build/sde \
      -DCMAKE_PREFIX_PATH=<path-with-nlohmann_json> \
      -DCMAKE_INSTALL_PREFIX=<install-prefix>
cmake --build build/sde
cmake --install build/sde
```

## Integration

SDE is consumed only through its public package contract:

```cmake
find_package(SDE CONFIG REQUIRED)
target_link_libraries(MyTarget PRIVATE SDE::Core)
```

`add_subdirectory(Tools/SystemDefinitionEngine)` is no longer supported as the
integration path — SDE is a packaged library, not an in-tree source drop.

### Engine integration

The engine links against SDE only when explicitly opted in:

```bash
# CMake
cmake -S . -B build -DSAGA_WITH_SDE=ON

# Conan (engine root)
conan install . -o &:with_sde=True
```

When `SAGA_WITH_SDE=OFF` (the default) the engine does not depend on SDE at
configure or link time.

## Source layout

```
Tools/SystemDefinitionEngine/
├── CMakeLists.txt         # standalone CMake project + install/export rules
├── conanfile.py           # Conan recipe — publishes sde/0.1.1
├── build.py               # bootstrap installer (Conan + CMake)
├── version.json           # package metadata
├── CHANGELOG.md
├── LICENSE
├── README.md
├── SDE_ROADMAP.md
├── cmake/
│   └── SDEConfig.cmake.in # find_package(SDE CONFIG) template
├── include/SDE/
│   ├── Compilation/       # CompiledModelGraph, ReferenceResolver, ModelCompiler
│   ├── Compiler/          # CompilerSession, dependency manifest
│   ├── Core/              # versioning, cancellation, stable hashing
│   ├── IO/                # ModelLoader (RawValue), JsonModelLoader, ModelWriter
│   ├── Model/             # TypeNode/Registry, EnumDefinition, FieldDefinition,
│   │                      #   EnumRegistry, Relation, ModelDefinition
│   └── Validation/        # Diagnostic, CompileState, ValidationResult,
│                          #   Predicate, Rule, Validator
├── src/SDE/               # Implementations mirror include/
├── cli/                   # sde CLI binary
└── tests/                 # GTest suite (opt-in: SDE_BUILD_TESTS)
```

## Strict isolation

SDE is held to the same loose-coupling contract as Forge and Prism:

- No `#include` of any `SagaEngine/`, `SagaEditor/`, `SagaServer/`,
  `SagaPrism/`, or `SagaTools/` header anywhere under
  `Tools/SystemDefinitionEngine/`.
- No `Saga*.cmake` module is consumed; this directory ships its own CMake
  build description.
- No transitive public coupling to engine third-party libraries. `nlohmann_json`
  is an implementation dependency for JSON load/write translation units, and
  `gtest` is test-only (`SDE_BUILD_TESTS=ON`).
- `nlohmann/json.hpp` is confined to `JsonModelLoader.cpp` and
  `ModelWriter.cpp` — it does not appear in any public header.

These invariants are verified by `Tools/Scripts/check_tools_isolation.py`.
Any violation blocks merge.

## Determinism and artifact identity

SDE output must be deterministic for the same normalized inputs. The compiled
graph uses deterministic model, instance, field, dependency, diagnostic, and
serialization ordering. Stable hashing normalizes line endings, repository-relative
paths, and path separators before computing fingerprints. Runtime-facing graph
data is immutable after a successful compile; editor mutation happens by changing
source data and recompiling.

The current artifact identity model includes schema, data, dependency, compiled
graph, and artifact hashes. Binary artifacts, incremental compilation, LSP, DSL,
streaming compile, arena allocation, and copy-on-write are reserved future work.
