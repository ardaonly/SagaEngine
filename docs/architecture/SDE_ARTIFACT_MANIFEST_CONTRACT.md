# SDE Artifact and Manifest Contract

This document records the current deterministic artifact and manifest contract
for the standalone System Definition Engine. It documents current behavior and
Phase 19 evidence boundaries; it does not add engine, editor, runtime, scripting,
Visual Blocks, package, or distribution ownership to SDE.

## Current Compile Outputs

`sde compile --workspace <path> --out <path>` currently writes:

- `manifest.json`: artifact metadata derived from `SDE::ArtifactManifest`;
- `diagnostics.json`: structured diagnostics emitted by the compiler pipeline;
- `hashes.json`: source, schema, data, dependency, graph, artifact, and model
  hashes;
- `graph.json`: compiled model graph output when compilation produces a usable
  graph.

The C++ API exposes the same contract through `SDE::CompilerFacadeResult`:

- `project`: compile state, graph, diagnostics, versions, dependencies, and
  hashes;
- `manifest`: artifact version, compiler version, language version, domain,
  kind, input/output hashes, model hashes, and output file entries.

Current manifest JSON uses `artifactVersion`; it does not currently emit a
separate `schemaVersion` field. Current diagnostics output is stable structured
compiler output, but the manifest's diagnostics output entry does not currently
carry a content hash.

## Manifest Shape

The current manifest contract is engine-neutral:

```json
{
  "artifactVersion": 1,
  "compilerVersion": "0.1.0",
  "languageVersion": 1,
  "domain": "Definition.Contract",
  "kind": "DefinitionGraph",
  "sourceHash": "<stable-input-hash>",
  "dependencyHash": "<stable-dependency-hash>",
  "modelHashes": {
    "ModelId": "<stable-model-hash>"
  },
  "outputs": [
    { "kind": "CompiledGraph", "path": "graph.json", "hash": "<graph-hash>" },
    { "kind": "Diagnostics", "path": "diagnostics.json", "hash": "" },
    { "kind": "Hashes", "path": "hashes.json", "hash": "<artifact-hash>" }
  ]
}
```

`domain` and `kind` identify a consumer domain and artifact type. They do not
make SDE depend on that consumer. Saga-specific adapters may interpret these
fields outside SDE Core.

## Determinism Rules

SDE artifact identity is deterministic when the same source inputs are compiled
with the same compiler version and artifact format:

- input discovery order is sorted;
- diagnostics are sorted before publication;
- dependency records use normalized workspace-relative paths;
- file fingerprints normalize CRLF by removing carriage returns;
- compiled graph hashes are produced through stable graph serialization;
- model hashes are stored in ordered maps;
- current deterministic artifacts do not include generation timestamps.

If future artifacts need timestamps or host-specific data, those fields must be
clearly marked non-deterministic and excluded from deterministic identity
checks.

## Boundary Rules

SDE Core emits generic definition/compiler artifacts. It must not depend on:

- SagaRuntime;
- SagaEditor;
- SagaServer;
- SagaScript or CSharpScriptHost;
- Visual Blocks editor, projection, or runtime graph execution;
- StarterArena gameplay;
- SagaEngine package/distribution internals.

Saga-specific consumers and adapters may consume `SDE::Core` outputs later.
That dependency direction must stay outside the SDE Core package.

## Phase 19 Verification

Phase 19 evidence is limited to the current standalone SDE package and tests.
The focused tests compile equivalent native `.sde` inputs twice and check:

- manifest version, domain, kind, source hash, dependency hash, model hashes, and
  output entries are stable;
- project source, schema, data, dependency, graph, artifact, and model hashes are
  stable;
- written `graph.json` output bytes match for equivalent inputs;
- invalid duplicate-instance input produces stable diagnostic code, message, and
  metadata;
- no engine/runtime/editor/server/script/Visual Blocks behavior is involved.

This is not SagaEngine package/distribution proof and does not mark any phase
`Verified`.
