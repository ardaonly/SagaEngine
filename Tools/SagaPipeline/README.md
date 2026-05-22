# SagaPipeline

SagaPipeline is the Saga-specific workflow orchestration layer.

It coordinates standalone tools for Saga product workflows without moving Saga
product knowledge into generic tools.

Ownership boundaries:

```txt
Tools/SagaPipeline
  owns Saga-specific workflow orchestration and product paths
  may invoke Forge, Prism, SDE, and SagaEditorComposition as external tools

Tools/Forge
  generic build frontend and process orchestration
  must not own SagaEditor composition domain defaults

Tools/Prism
  generic code and artifact intelligence
  must not own Saga product pipeline policy

Tools/SystemDefinitionEngine
  generic deterministic definition compiler
  must not contain Saga-specific schemas or adapters

Tools/SagaTools
  thin dispatcher
  must not own orchestration logic
```

Current V1 command:

```txt
saga-pipeline editor-composition build --project .
```

This invokes `saga-editor-composition-compiler` as an external process and
passes the SagaEditor product workspace and build root. SagaPipeline does not
parse `.sde` files, inspect SDE internals, write composition artifacts, link
SagaEditor runtime code, or call Apps/Editor.

For the SDE-enabled editor/product toolchain profile, build the compiler through
the normal SagaEngine CMake graph and pass its executable path explicitly:

```sh
Tools/Forge/bin/forge nix run conan create Tools/SystemDefinitionEngine --build=missing
Tools/Forge/bin/forge nix install --profile linux-gcc-sde
Tools/Forge/bin/forge nix configure --preset linux-gcc-sde
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo-sde -j 1

build/RelWithDebInfo-sde/bin/saga-pipeline editor-composition build \
  --project . \
  --tool build/RelWithDebInfo-sde/bin/saga-editor-composition-compiler
```

The default generic build may remain SDE-off. SagaPipeline stays a process
orchestrator and does not link SagaEditorComposition internals.
