# Saga Schema Package Boundary



## Current Reality

There is no dedicated Saga schema package tree in this repository today. In
particular, Phase 20 does not create:





- `docs/architecture/SDE_CURRENT_CONTRACT.md`;
- `docs/architecture/SDE_ARTIFACT_MANIFEST_CONTRACT.md`.







## Dependency Direction

The dependency direction is one-way:







- SagaRuntime;
- SagaEditor;
- SagaServer;
- SagaScript or CSharpScriptHost;
- Visual Blocks editor/projection/runtime graph implementation;
- StarterArena gameplay;
- Saga package/distribution internals.

## Boundary Model

The intended model is:





Saga Consumers
  Runtime/editor/product/package tools
  Read Saga-owned package artifacts when those artifacts exist
```



## Future Package Shape



```txt
<saga-schema-package>/
  manifest.json
  schemas/
  adapters/
  templates/
  validators/
```

This is a future package shape, not current implementation. Phase 20 does not
claim package readiness or distribution readiness.

## Verification Boundary

Phase 20 evidence is limited to:



Distribution readiness, package staging, runtime/editor consumption, and final
schema package layout remain future work.
