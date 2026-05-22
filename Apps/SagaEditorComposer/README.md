# SagaEditorComposer

SagaEditorComposer is the visual authoring app for SagaEditor composition
source. It edits canonical source under:

```txt
Editor/CompositionSources/source/*.sde
```

Generated files under `Build/Artifacts`, `Build/Manifests`, and
`Build/Reports` are read-only build outputs. They are never treated as source
truth by Composer.

## Boundaries

- `Apps/SagaEditorComposer` owns the visual authoring surface.
- `Tools/SagaEditorComposition` owns headless `.sde` to artifact compilation.
- `Tools/SagaPipeline` owns Saga-specific workflow orchestration.
- Composer invokes SagaPipeline as an external process when available.
- Composer does not include SDE, Forge, SagaPipeline, or compiler internals.

## MVP Editing

The MVP source indexer recognizes saga.editor-style `instance` blocks and simple
scalar field assignments. Supported safe edits are:

- `displayName` on composition, panel, action, menu, toolbar, workspace, and mode
  definitions.
- `defaultVisible` on panel definitions.

Save creates checkpoints under:

```txt
<workspace>/.saga/composer/checkpoints/
```

Full graph editing, adding or removing definitions, schema migration, plugin
loading, and runtime hot reload are future work.

## CLI

```sh
SagaEditorComposer --workspace .
SagaEditorComposer --smoke --workspace . --no-show --smoke-timeout-ms 100
SagaEditorComposer --help
```

## Qt Runtime

The default Conan Qt package is linked statically. SagaEditorComposer imports the
static xcb platform plugin for visible Linux windows and the offscreen platform
plugin for smoke tests. The current qtbase-only package graph does not provide a
native QtWayland platform plugin, so Wayland sessions use xcb/XWayland unless a
future toolchain profile explicitly adds QtWayland.

On NixOS or other minimal shells, run visible Qt apps from an environment with
the required XCB/XWayland native libraries and a UTF-8 locale, for example:

```sh
export LANG=C.UTF-8
export LC_ALL=C.UTF-8
SagaEditorComposer --workspace .
```
