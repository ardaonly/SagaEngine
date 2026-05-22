# SagaEditorComposition

SagaEditorComposition is the Saga-owned compiler adapter for editor composition
data.

It compiles the checked-in SagaEditor composition workspace:

```txt
Editor/CompositionSources/source/*.sde
```

into deterministic editor composition artifacts, manifests, diagnostics, source
maps, and dependency manifests under the product `Build` tree.

## Ownership

```txt
Tools/SagaEditorComposition
  owns saga.editor .sde -> editor composition artifact generation

Tools/SystemDefinitionEngine
  owns the generic SDE compiler and public facade APIs

Tools/SagaPipeline
  invokes saga-editor-composition-compiler as an external process

Tools/Forge
  may build the executable through CMake, but does not own the domain workflow

Apps/Editor
  consumes compiled manifests and artifacts at startup, but never invokes this tool
```

The adapter may depend on SDE public package APIs. It must not live under the SDE
package tree and must not depend on Qt, Apps/Editor, Forge internals, Prism
internals, or SagaPipeline internals.

## Build Policy

The global SagaEngine default remains SDE-off. The compiler is produced by the
SDE-enabled editor/product toolchain profile:

```sh
Tools/Forge/bin/forge nix run conan create Tools/SystemDefinitionEngine --build=missing
Tools/Forge/bin/forge nix install --profile linux-gcc-sde
Tools/Forge/bin/forge nix configure --preset linux-gcc-sde
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo-sde -j 1
```

Expected executable:

```txt
build/RelWithDebInfo-sde/bin/saga-editor-composition-compiler
```

When `SAGA_WITH_SDE=OFF`, no dummy compiler executable is produced.

## Usage

```sh
build/RelWithDebInfo-sde/bin/saga-editor-composition-compiler --help

build/RelWithDebInfo-sde/bin/saga-editor-composition-compiler compile \
  --workspace Editor/CompositionSources \
  --build-root Build \
  --composition-id saga.editor.default
```

SagaPipeline should invoke the produced executable as an external tool:

```sh
build/RelWithDebInfo-sde/bin/saga-pipeline editor-composition build \
  --project . \
  --tool build/RelWithDebInfo-sde/bin/saga-editor-composition-compiler
```
