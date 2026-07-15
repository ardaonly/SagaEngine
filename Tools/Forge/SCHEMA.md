# Forge Schema Reference

> Last updated: 2026-05-15
> Status: Proposed schema reference
> Target: A stable, conservative `forge.toml` schema for Forge's standalone C++ workflow and Saga build/publish pipeline integration.

---

## 0. Schema Philosophy

`forge.toml` is not a new programming language.

It is a build workflow manifest.

Its job is to describe enough project/tool/build/pipeline configuration for Forge to produce a deterministic `BuildModel`, lower that into a `BuildPlan`, invoke tool adapters, validate outputs, stage packages, and write reports.

It must not become:

* a CMake replacement,
* a Conan replacement,
* a script language,
* an asset pipeline language,
* a package scripting language,
* a product project manifest replacement,
* a dumping ground for every prototype flag.

Correct model:

```txt
forge.toml
    ↓
Forge manifest parser
    ↓
BuildModel
    ↓
BuildPlan
    ↓
Adapters / steps / reports
```

Incorrect model:

```txt
forge.toml contains half the engine architecture because adding TOML fields felt easier than writing real owners.
```

That road is paved. It leads to a build file nobody wants to touch.

---

## 1. Compatibility Rule

Forge must support two project classes:

```txt
Standalone C++ project
Saga project
```

Standalone C++ project:

```txt
forge.toml
CMakeLists.txt
conanfile.py / conanfile.txt where applicable
```

Saga project:

```txt
saga.project.json
forge.toml
Scripts/
Assets/
Build/
Packages/
```

Rule:

```txt
Saga-specific sections are optional.
Standalone C++ projects must not be forced to define Saga pipeline sections.
```

---

## 2. Versioning

Every `forge.toml` should include a schema version.

```toml
schemaVersion = 1
```

Rules:

* Missing `schemaVersion` may be treated as legacy schema version `0`.
* Unsupported future versions must fail clearly under `--strict`.
* Unknown sections may be preserved by tools, but `--strict` should reject unsupported active sections.
* Reserved sections may be recognized but ignored unless enabled by Forge version/profile.

Example diagnostic:

```txt
Forge.Schema.UnsupportedVersion:
forge.toml uses schemaVersion = 3, but this Forge supports schemaVersion <= 1.
```

---

## 3. Current Minimal Schema

The conservative baseline should keep these sections stable:

```toml
schemaVersion = 1

[project]
name = "SagaEngine"
version = "0.0.7"
language = "cpp"

[toolchain]
cmake = ">=3.26"
conan = ">=2.0"
compiler = "clang"
cppStandard = "20"

[build]
backend = "cmake"
buildDir = "Build"
defaultConfig = "Debug"
defaultTarget = "all"

[deps]
mode = "conan"
```

This is the stable base.

Everything else should be added only when the implementation actually exists.

---

## 4. Top-Level Keys

Allowed top-level keys:

```toml
schemaVersion = 1
```

Allowed top-level sections:

```toml
[project]
[toolchain]
[build]
[deps]
[env]
[profiles]
[saga]
[scripts]
[assets]
[package]
[publish]
[reports]
```

Section status:

| Section       | Status            | Purpose                                         |
| ------------- | ----------------- | ----------------------------------------------- |
| `[project]`   | Active            | General project metadata                        |
| `[toolchain]` | Active            | Required external tool versions/pins            |
| `[build]`     | Active            | Backend/build directory/config defaults         |
| `[deps]`      | Active            | Dependency manager/install mode                 |
| `[env]`       | Active            | Tool executable overrides and environment hints |
| `[profiles]`  | Proposed          | Named build profiles                            |
| `[saga]`      | Proposed          | Optional Saga project integration               |
| `[scripts]`   | Reserved/Proposed | Optional scripting pipeline roots/options       |
| `[assets]`    | Reserved/Proposed | Optional asset pipeline roots/options           |
| `[package]`   | Reserved/Proposed | Optional package staging options                |
| `[publish]`   | Reserved/Proposed | Optional publish-check policy                   |
| `[reports]`   | Proposed          | Report output paths/options                     |

Important:

```txt
Proposed means schema design is acceptable.
Implemented behavior still requires Forge code.
Reserved means do not rely on it yet.
```

---

## 5. `[project]`

Purpose:

General Forge project metadata.

Example:

```toml
[project]
name = "SagaEngine"
version = "0.0.7"
language = "cpp"
description = "SagaEngine runtime and tools"
```

Fields:

| Field         |   Type | Required | Status | Meaning                            |
| ------------- | -----: | -------: | ------ | ---------------------------------- |
| `name`        | string |      yes | Active | Project display/package name       |
| `version`     | string |       no | Active | Project version                    |
| `language`    | string |       no | Active | Primary project language hint      |
| `description` | string |       no | Active | Human-readable project description |

Allowed `language` examples:

```txt
cpp
cpp-saga
mixed
```

Rules:

* `name` must be stable and filesystem-safe where used for generated paths.
* `version` should be semantic where practical.
* `language` is a hint, not ownership truth.

---

## 6. `[toolchain]`

Purpose:

Declare required toolchain constraints.

Example:

```toml
[toolchain]
cmake = ">=3.26"
conan = ">=2.0"
compiler = "clang"
cppStandard = "20"
ninja = ">=1.11"
```

Fields:

| Field             |       Type | Required | Status   | Meaning                             |
| ----------------- | ---------: | -------: | -------- | ----------------------------------- |
| `cmake`           |     string |       no | Active   | Required CMake version constraint   |
| `conan`           |     string |       no | Active   | Required Conan version constraint   |
| `compiler`        |     string |       no | Active   | Compiler family hint/pin            |
| `compilerVersion` |     string |       no | Proposed | Compiler version constraint         |
| `cppStandard`     | string/int |       no | Active   | Required C++ standard               |
| `ninja`           |     string |       no | Proposed | Ninja version constraint            |
| `forge`           |     string |       no | Proposed | Forge version constraint            |
| `dotnet`          |     string |       no | Reserved | .NET SDK/script compiler constraint |
| `assetPipeline`   |     string |       no | Reserved | Asset pipeline tool version         |

Rules:

* `--strict` must validate configured tool constraints.
* `--strict --frozen` must compare relevant tool versions against lock data where supported.
* Missing optional tools should not fail standalone C++ workflows unless required by selected profile.

Bad:

```toml
[toolchain]
compiler = "whatever works"
```

Good:

```toml
[toolchain]
compiler = "clang"
cppStandard = "20"
cmake = ">=3.26"
```

---

## 7. `[build]`

Purpose:

Configure backend-neutral build defaults.

Example:

```toml
[build]
backend = "cmake"
buildDir = "Build"
defaultConfig = "Debug"
defaultTarget = "all"
generator = "Ninja"
```

Fields:

| Field              |   Type | Required | Status   | Meaning                        |
| ------------------ | -----: | -------: | -------- | ------------------------------ |
| `backend`          | string |       no | Active   | Build backend, usually `cmake` |
| `buildDir`         | string |       no | Active   | Build output directory         |
| `defaultConfig`    | string |       no | Active   | Default build configuration    |
| `defaultTarget`    | string |       no | Active   | Default target                 |
| `generator`        | string |       no | Active   | CMake generator hint           |
| `installPrefix`    | string |       no | Proposed | Install prefix                 |
| `compileCommands`  |   bool |       no | Proposed | Generate compile_commands.json |
| `warningsAsErrors` |   bool |       no | Proposed | Build strictness hint          |

Allowed `backend` values:

```txt
cmake
```

Future possible values:

```txt
cargo
msbuild
custom
```

Rules:

* CMake-specific details should remain minimal.
* Complex CMake logic belongs in `CMakeLists.txt`, not `forge.toml`.
* Forge may pass configured values to backend adapters.

---

## 8. `[deps]`

Purpose:

Configure dependency installation mode.

Example:

```toml
[deps]
mode = "conan"
lockfile = "forge.lock"
```

Fields:

| Field                |   Type | Required | Status   | Meaning                                            |
| -------------------- | -----: | -------: | -------- | -------------------------------------------------- |
| `mode`               | string |       no | Active   | Dependency mode                                    |
| `lockfile`           | string |       no | Proposed | Lockfile path                                      |
| `installOnConfigure` |   bool |       no | Proposed | Whether Forge should install deps before configure |
| `profileHost`        | string |       no | Proposed | Conan host profile                                 |
| `profileBuild`       | string |       no | Proposed | Conan build profile                                |

Allowed `mode` values:

```txt
none
conan
system
```

Rules:

* `conan` mode routes through `ConanAdapter`.
* `none` means Forge should not attempt dependency install.
* `system` means dependencies are expected from system/toolchain environment.
* `--strict --frozen` should validate lockfile behavior where supported.

---

## 9. `[env]`

Purpose:

Configure tool executable overrides and environment hints.

Example:

```toml
[env]
cmake = "/usr/bin/cmake"
conan = "/home/user/.local/bin/conan"
```

Fields:

| Field            |   Type | Required | Status   | Meaning                             |
| ---------------- | -----: | -------: | -------- | ----------------------------------- |
| `cmake`          | string |       no | Active   | CMake executable override           |
| `ctest`          | string |       no | Active   | CTest executable override           |
| `conan`          | string |       no | Active   | Conan executable override           |
| `ninja`          | string |       no | Proposed | Ninja executable override           |
| `assetPipeline`  | string |       no | Reserved | Asset pipeline executable override  |
| `scriptCompiler` | string |       no | Reserved | Script compiler executable override |
| `host`           | string |       no | Proposed | Host tool executable override       |

Rules:

* Forge's `ToolEnv` should resolve executable paths.
* Hardcoded tool executable names inside build steps should be avoided.
* Environment overrides should be visible through `forge env`.

---

## 10. `[profiles]`

Purpose:

Declare named Forge build profiles.

Status: Proposed.

Example:

```toml
[profiles.editor-evaluation]
config = "Debug"
target = "SagaEditor"
strict = false
scripts = true
assets = true
package = "editor-evaluation"

[profiles.ci]
config = "Debug"
target = "all"
strict = true
scripts = true
assets = true
package = false

[profiles.shipping-full]
config = "Release"
target = "all"
strict = true
scripts = true
assets = true
package = "shipping-full"
publishCheck = true
```

Required profile names for Saga projects:

```txt
editor-evaluation
dev-client
dev-server
dev-local-client-server
test
ci
shipping-client
shipping-server
shipping-full
```

Profile fields:

| Field               |        Type | Required | Status            | Meaning                             |
| ------------------- | ----------: | -------: | ----------------- | ----------------------------------- |
| `config`            |      string |       no | Proposed          | Build config                        |
| `target`            |      string |       no | Proposed          | Backend target                      |
| `strict`            |        bool |       no | Proposed          | Enable strict validation            |
| `frozen`            |        bool |       no | Proposed          | Enable frozen checks                |
| `scripts`           | bool/string |       no | Reserved/Proposed | Enable script step/config           |
| `assets`            | bool/string |       no | Reserved/Proposed | Enable asset step/config            |
| `package`           | bool/string |       no | Proposed          | Package profile/kind                |
| `publishCheck`      |        bool |       no | Proposed          | Run publish readiness check         |
| `collaborationGate` | bool/string |       no | Reserved          | Collaboration gate policy           |

Rules:

* Profiles describe which steps are active.
* Step implementation remains in Forge pipeline/adapters.
* Profile names should be stable because CI/editor/product workflows may reference them.
* Missing optional sections should produce clear diagnostics when a profile enables them.

---

## 11. `[saga]`

Purpose:

Optional bridge to a Saga project manifest.

Status: Proposed.

Example:

```toml
[saga]
projectManifest = "saga.project.json"
workspaceRoot = "."
defaultProfile = "editor-evaluation"
```

Fields:

| Field                |   Type | Required | Status   | Meaning                       |
| -------------------- | -----: | -------: | -------- | ----------------------------- |
| `projectManifest`    | string |       no | Proposed | Path to `saga.project.json`   |
| `workspaceRoot`      | string |       no | Proposed | Workspace root override       |
| `defaultProfile`     | string |       no | Proposed | Default build profile         |
| `requireSagaProject` |   bool |       no | Proposed | Fail if Saga manifest missing |

Rules:

* `saga.project.json` remains product/project manifest truth.
* `forge.toml` may reference it but should not duplicate all project metadata.
* Standalone C++ projects should omit `[saga]`.

Bad:

```toml
[saga]
# giant duplicate of entire saga.project.json
```

Good:

```toml
[saga]
projectManifest = "saga.project.json"
defaultProfile = "editor-evaluation"
```

---


Purpose:


Status: Proposed.

Example:

```toml
mode = "compile"
```

Fields:

| Field            |         Type | Required | Status   | Meaning                                                 |
| ---------------- | -----------: | -------: | -------- | ------------------------------------------------------- |
| `mode`           |       string |       no | Proposed | `validate` or `compile`                                 |
| `schemaPackages` | array/string |       no | Reserved | Extra schema package roots                              |

Allowed `mode` values:

```txt
validate
compile
```

Rules:


---

## 13. `[scripts]`

Purpose:

Configure script validation/compile/binding generation.

Status: Reserved/Proposed.

Do not rely on this section until scripting toolchain implementation stabilizes.

Example:

```toml
[scripts]
root = "Scripts"
out = "Build/Artifacts/Scripts"
bindings = "Build/Manifests/script_bindings.json"
diagnostics = "Build/Reports/script_diagnostics.json"
language = "csharp"
```

Fields:

| Field           |   Type | Required | Status            | Meaning                             |
| --------------- | -----: | -------: | ----------------- | ----------------------------------- |
| `root`          | string |       no | Reserved/Proposed | Script source root                  |
| `out`           | string |       no | Reserved/Proposed | Script artifact output directory    |
| `bindings`      | string |       no | Reserved/Proposed | Binding manifest output             |
| `diagnostics`   | string |       no | Reserved/Proposed | Script diagnostics report           |
| `language`      | string |       no | Reserved/Proposed | Script language, initially `csharp` |
| `generatedRoot` | string |       no | Reserved          | Generated code root                 |
| `strict`        |   bool |       no | Reserved          | Strict script validation            |

Rules:

* Forge invokes scripting toolchain through adapter boundary.
* Forge does not become the C# compiler.
* Runtime/server consume script artifacts and binding manifests, not script compiler internals.

---

## 14. `[assets]`

Purpose:

Configure asset validation/cook/artifact generation.

Status: Reserved/Proposed.

Do not rely on this section until AssetPipeline implementation stabilizes.

Example:

```toml
[assets]
root = "Assets"
out = "Build/Artifacts/Assets"
manifest = "Build/Manifests/assets.json"
diagnostics = "Build/Reports/asset_diagnostics.json"
profile = "dev"
```

Fields:

| Field            |   Type | Required | Status            | Meaning                          |
| ---------------- | -----: | -------: | ----------------- | -------------------------------- |
| `root`           | string |       no | Reserved/Proposed | Source asset root                |
| `out`            | string |       no | Reserved/Proposed | Cooked artifact output directory |
| `manifest`       | string |       no | Reserved/Proposed | Asset manifest output            |
| `diagnostics`    | string |       no | Reserved/Proposed | Asset diagnostics report         |
| `profile`        | string |       no | Reserved/Proposed | Cook profile                     |
| `targetPlatform` | string |       no | Reserved          | Target platform                  |
| `strict`         |   bool |       no | Reserved          | Strict asset validation          |

Rules:

* AssetPipeline owns import/cook implementation.
* Forge invokes AssetPipeline through adapter boundary.
* Runtime consumes cooked artifacts and manifests.
* Runtime does not import arbitrary source assets in shipping mode.

---


Purpose:


Status: Proposed.

Example:

```toml
enabled = true
reports = "Build/Reports"
boundaries = true
stale = true
packages = true
generatedOrigin = true
```

Fields:

| Field             |   Type | Required | Status   | Meaning                        |
| ----------------- | -----: | -------: | -------- | ------------------------------ |
| `boundaries`      |   bool |       no | Proposed | Run boundary checks            |
| `stale`           |   bool |       no | Proposed | Run stale artifact checks      |
| `packages`        |   bool |       no | Proposed | Run package consistency checks |
| `generatedOrigin` |   bool |       no | Proposed | Run generated origin checks    |

Rules:


---

## 16. `[package]`

Purpose:

Configure package staging.

Status: Reserved/Proposed.

Example:

```toml
[package]
out = "Packages"
default = "editor-evaluation"

[package.profiles.editor-evaluation]
kind = "editor-evaluation"
out = "Packages/editor-evaluation"

[package.profiles.dev-client]
kind = "client"
out = "Packages/dev-client"

[package.profiles.dev-server]
kind = "server"
out = "Packages/dev-server"

[package.profiles.shipping-client]
kind = "client"
out = "Packages/shipping-client"
strict = true

[package.profiles.shipping-server]
kind = "server"
out = "Packages/shipping-server"
strict = true
```

Top-level fields:

| Field     |   Type | Required | Status            | Meaning                 |
| --------- | -----: | -------: | ----------------- | ----------------------- |
| `out`     | string |       no | Reserved/Proposed | Package output root     |
| `default` | string |       no | Reserved/Proposed | Default package profile |

Package profile fields:

| Field                |   Type | Required | Status            | Meaning                              |
| -------------------- | -----: | -------: | ----------------- | ------------------------------------ |
| `kind`               | string |      yes | Reserved/Proposed | Package kind                         |
| `out`                | string |       no | Reserved/Proposed | Output directory                     |
| `strict`             |   bool |       no | Reserved/Proposed | Strict staging validation            |
| `manifest`           | string |       no | Reserved/Proposed | Package manifest output path         |
| `includeDiagnostics` |   bool |       no | Reserved          | Include allowed diagnostics metadata |

Allowed package kinds:

```txt
editor-evaluation
client
server
full
```

Rules:

* Forge owns package staging.
* Runtime/server consume package manifests.
* Package placement must respect authority metadata.
* Server-only executable artifacts must not enter client package.

---

## 17. `[publish]`

Purpose:

Configure publish readiness checks.

Status: Reserved/Proposed.

Example:

```toml
[publish]
defaultProfile = "shipping-full"
report = "Build/Reports/publish_report.json"
requireFreshAssets = true
requireCleanCollaboration = true
```

Fields:

| Field                       |   Type | Required | Status            | Meaning                          |
| --------------------------- | -----: | -------: | ----------------- | -------------------------------- |
| `defaultProfile`            | string |       no | Reserved/Proposed | Default publish profile          |
| `report`                    | string |       no | Reserved/Proposed | Publish report path              |
| `requireFreshAssets`        |   bool |       no | Reserved/Proposed | Block stale cooked assets        |
| `requireCleanCollaboration` |   bool |       no | Reserved          | Block unresolved conflicts/locks |
| `allowWarnings`             |   bool |       no | Reserved          | Allow publish with warnings      |

Rules:

* `publish-check` does not necessarily publish/upload anything.
* Publish readiness reports blockers.
* Actual deployment/release is outside this schema unless a future deployment roadmap defines it.

---

## 18. `[reports]`

Purpose:

Configure report output locations.

Status: Proposed.

Example:

```toml
[reports]
root = "Build/Reports"
build = "Build/Reports/build_report.json"
diagnostics = "Build/Reports/diagnostics_report.json"
publish = "Build/Reports/publish_report.json"
```

Fields:

| Field               |   Type | Required | Status   | Meaning                          |
| ------------------- | -----: | -------: | -------- | -------------------------------- |
| `root`              | string |       no | Proposed | Default report root              |
| `build`             | string |       no | Proposed | Build report path                |
| `diagnostics`       | string |       no | Proposed | Combined diagnostics report path |
| `publish`           | string |       no | Proposed | Publish report path              |
| `assetDiagnostics`  | string |       no | Reserved | Asset diagnostics report path    |
| `scriptDiagnostics` | string |       no | Reserved | Script diagnostics report path   |

Rules:

* Reports should be machine-readable JSON by default.
* CLI may print human-readable summaries, but terminal output is not the only source of truth.
* Report paths should be stable for CI/editor/product integration.

---

## 19. Unknown Sections and Strict Mode

Default behavior:

* Unknown top-level sections may be ignored with warning in non-strict mode.
* Unknown top-level sections must fail under `--strict`, unless explicitly declared as reserved/recognized.
* Unknown fields inside known sections should warn in non-strict mode and fail in strict mode.

Example:

```toml
[magic]
doEverything = true
```

Strict diagnostic:

```txt
Forge.Schema.UnknownSection:
Unknown section [magic]. Forge does not support this section.
```

Non-strict diagnostic:

```txt
Forge.Schema.UnknownSectionWarning:
Ignoring unknown section [magic].
```

---

## 20. Reserved Sections Policy

Reserved sections are allowed as design placeholders but not stable user-facing API.

Rules:

* Reserved fields must not be required by stable workflows.
* Reserved fields may be rejected by older Forge versions.
* Documentation must clearly mark them.
* CI should not depend on reserved fields unless the implementation lands.

Reserved/proposed sections in this schema:

```txt
[scripts]
[assets]
[package]
[publish]
```

These are important, but early schema promises are expensive.

Better to mark them honestly than pretend a non-existent pipeline is stable.

---

## 21. Example: Standalone C++ Project

```toml
schemaVersion = 1

[project]
name = "ExampleCppProject"
version = "0.1.0"
language = "cpp"

[toolchain]
cmake = ">=3.26"
conan = ">=2.0"
compiler = "clang"
cppStandard = "20"

[build]
backend = "cmake"
buildDir = "Build"
defaultConfig = "Debug"
defaultTarget = "all"
generator = "Ninja"

[deps]
mode = "conan"
```

Expected commands:

```txt
forge install
forge configure
forge build
forge test
```


---

## 22. Example: Minimal Saga Project

```toml
schemaVersion = 1

[project]
name = "ExampleSagaProject"
version = "0.1.0"
language = "cpp-saga"

[toolchain]
cmake = ">=3.26"
conan = ">=2.0"
compiler = "clang"
cppStandard = "20"

[build]
backend = "cmake"
buildDir = "Build"
defaultConfig = "Debug"
defaultTarget = "all"
generator = "Ninja"

[deps]
mode = "conan"

[saga]
projectManifest = "saga.project.json"
defaultProfile = "editor-evaluation"

mode = "compile"

enabled = true
reports = "Build/Reports"
boundaries = true
stale = true
packages = true

[reports]
root = "Build/Reports"
build = "Build/Reports/build_report.json"
diagnostics = "Build/Reports/diagnostics_report.json"
publish = "Build/Reports/publish_report.json"

[profiles.editor-evaluation]
config = "Debug"
target = "SagaEditor"
strict = false
package = "editor-evaluation"

[profiles.ci]
config = "Debug"
target = "all"
strict = true
package = false

[profiles.shipping-full]
config = "Release"
target = "all"
strict = true
package = "shipping-full"
publishCheck = true
```

This is still conservative.

Scripts/assets/package/publish sections are intentionally not fully required until implementation exists.

---

## 23. Example: Future Full Saga Pipeline

Status: future-oriented, not stable until implemented.

```toml
schemaVersion = 1

[project]
name = "ExampleMMO"
version = "0.1.0"
language = "cpp-saga"

[toolchain]
cmake = ">=3.26"
conan = ">=2.0"
compiler = "clang"
cppStandard = "20"
dotnet = ">=8.0"
assetPipeline = ">=0.1.0"

[build]
backend = "cmake"
buildDir = "Build"
defaultConfig = "Debug"
generator = "Ninja"
compileCommands = true

[deps]
mode = "conan"
lockfile = "forge.lock"

[saga]
projectManifest = "saga.project.json"
defaultProfile = "editor-evaluation"

mode = "compile"

[scripts]
root = "Scripts"
out = "Build/Artifacts/Scripts"
bindings = "Build/Manifests/script_bindings.json"
diagnostics = "Build/Reports/script_diagnostics.json"
language = "csharp"

[assets]
root = "Assets"
out = "Build/Artifacts/Assets"
manifest = "Build/Manifests/assets.json"
diagnostics = "Build/Reports/asset_diagnostics.json"
profile = "dev"

enabled = true
reports = "Build/Reports"
boundaries = true
stale = true
packages = true
generatedOrigin = true

[package]
out = "Packages"
default = "editor-evaluation"

[package.profiles.editor-evaluation]
kind = "editor-evaluation"
out = "Packages/editor-evaluation"

[package.profiles.dev-client]
kind = "client"
out = "Packages/dev-client"
manifest = "Build/Manifests/package_manifest.client.json"

[package.profiles.dev-server]
kind = "server"
out = "Packages/dev-server"
manifest = "Build/Manifests/package_manifest.server.json"

[publish]
defaultProfile = "shipping-full"
report = "Build/Reports/publish_report.json"
requireFreshAssets = true
requireCleanCollaboration = true

[reports]
root = "Build/Reports"
build = "Build/Reports/build_report.json"
diagnostics = "Build/Reports/diagnostics_report.json"
publish = "Build/Reports/publish_report.json"

[profiles.dev-local-client-server]
config = "Debug"
target = "all"
strict = false
scripts = true
assets = true
package = "dev-local-client-server"

[profiles.shipping-full]
config = "Release"
target = "all"
strict = true
frozen = true
scripts = true
assets = true
package = "shipping-full"
publishCheck = true
collaborationGate = "strict"
```

Use this as a direction, not as stable promise until Forge pipeline implementation catches up.

---

## 24. Generated Paths Convention

Recommended paths:

```txt
Build/Artifacts/Scripts/
Build/Artifacts/Assets/
Build/Manifests/
Build/Reports/
Packages/editor-evaluation/
Packages/dev-client/
Packages/dev-server/
Packages/shipping-client/
Packages/shipping-server/
```

Recommended report paths:

```txt
Build/Reports/build_report.json
Build/Reports/diagnostics_report.json
Build/Reports/publish_report.json
Build/Reports/script_diagnostics.json
Build/Reports/asset_diagnostics.json
```

Rules:

* Generated artifacts should stay out of source roots.
* Reports should be stable enough for editor/CI/product consumption.
* Package outputs should be separable by profile/kind.

---

## 25. Lockfile Interaction

`forge.lock` records resolved dependency/tool/build state where supported.

Lockfile may eventually record:

```txt
dependency install mode
dependency versions
tool versions
schema package versions
script compiler version
asset pipeline version
package profile versions
profile hash
```

Rules:

* `--strict --frozen` must reject drift against lockfile where implemented.
* Lockfile should be human-inspectable.
* Lockfile should not contain secrets.
* Lockfile should not be treated as a replacement for manifests/reports.

---

## 26. Validation Rules

Forge should validate:

```txt
schemaVersion supported
known sections
known fields
required fields
field types
path fields are valid strings
profile references exist
package profile references exist
enabled tool sections have required tool availability
report paths are writable
build/profile combinations are supported
```

Under `--strict`, Forge should additionally validate:

```txt
unknown sections rejected
unknown fields rejected
toolchain pins satisfied
lockfile satisfied where frozen
profile required tools available
reserved unstable fields rejected unless feature-enabled
```

---

## 27. Diagnostics

Schema diagnostics should use stable codes.

Examples:

```txt
Forge.Schema.UnsupportedVersion
Forge.Schema.UnknownSection
Forge.Schema.UnknownField
Forge.Schema.InvalidType
Forge.Schema.MissingRequiredField
Forge.Schema.InvalidProfileReference
Forge.Schema.ReservedFieldUsed
Forge.Schema.ToolRequiredButMissing
Forge.Schema.ReportPathNotWritable
```

Bad diagnostic:

```txt
Invalid toml.
```

Good diagnostic:

```txt
Forge.Schema.InvalidProfileReference:
profiles.shipping-full.package references package profile "shipping-full", but [package.profiles.shipping-full] is not defined.
```

---

## 28. Implementation Notes

Expected parser output:

```txt
ForgeManifest
    ↓
BuildModel
    ↓
BuildPlan
```

Suggested files:

```txt
Tools/Forge/include/Forge/Manifest/ForgeManifest.hpp
Tools/Forge/include/Forge/Manifest/ForgeManifestParser.hpp
Tools/Forge/include/Forge/Manifest/ForgeManifestValidator.hpp
Tools/Forge/include/Forge/Manifest/ForgeSchemaVersion.hpp
Tools/Forge/src/Manifest/ForgeManifestParser.cpp
Tools/Forge/src/Manifest/ForgeManifestValidator.cpp
```

Suggested tests:

```txt
Tools/Forge/tests/ForgeManifestParserTests.cpp
Tools/Forge/tests/ForgeManifestValidatorTests.cpp
Tools/Forge/tests/ForgeSchemaStrictModeTests.cpp
Tools/Forge/tests/ForgeProfileSchemaTests.cpp
```

---

## 29. Non-Goals

This schema must not define:

* CMake target graph internals,
* Conan dependency graph internals,
* gameplay graph node syntax,
* C# compile rules in detail,
* asset importer/cooker internals,
* runtime/server execution logic,
* collaboration conflict resolution behavior,
* product UI layout,
* editor panel layout,
* deployment provider configuration.

Those belong to their owners.

`forge.toml` wires workflow configuration.

It should not become the place where ownership disappears.

---

## 30. Decision Summary

Preserve these decisions:

```txt
forge.toml is a build workflow manifest.
Forge lowers forge.toml into BuildModel and BuildPlan.
Standalone C++ projects remain supported.
Saga-specific sections are optional.
Reserved sections must be honestly marked until implementation exists.
Unknown fields fail under --strict.
Reports and manifests are first-class outputs.
forge.toml must not duplicate saga.project.json entirely.
```

The schema should grow slowly.

Fast schema growth feels productive until every field becomes legacy.

A boring manifest that survives is better than an impressive manifest that lies.
