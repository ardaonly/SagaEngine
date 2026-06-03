# Phase 19 Evidence

## Status

Implemented-Unverified

## Phase Scope

SDE Artifact and Manifest Contracts

Phase 19 defines and tests the current deterministic artifact/manifest contract
for the standalone System Definition Engine. It does not modify SagaScript,
runtime, server, editor, package/distribution, StarterArena gameplay,
CSharpScriptHost, or Visual Blocks implementation.

## Contract Document

This phase adds:

- `docs/architecture/SDE_ARTIFACT_MANIFEST_CONTRACT.md`

The document records current SDE compile outputs:

- `manifest.json`;
- `diagnostics.json`;
- `hashes.json`;
- `graph.json` when a usable compiled graph exists.

It also records the current manifest fields:

- `artifactVersion`;
- `compilerVersion`;
- `languageVersion`;
- `domain`;
- `kind`;
- `sourceHash`;
- `dependencyHash`;
- `modelHashes`;
- `outputs[]`.

Current reality: SDE emits `artifactVersion`, not a separate `schemaVersion`
field, and the manifest's diagnostics output entry currently has an empty hash.

## Determinism Evidence

`Tools/SystemDefinitionEngine/tests/Compiler/CompilerSessionTests.cpp` now
contains focused artifact contract checks:

- equivalent native `.sde` source compiled from two temp workspaces produces
  stable manifest identity;
- output entries for `graph.json`, `diagnostics.json`, and `hashes.json` are
  present and ordered;
- source, schema, data, dependency, graph, artifact, and model hashes match
  across equivalent inputs;
- `JsonModelWriter` produces byte-identical `graph.json` output for equivalent
  compiled graphs;
- invalid duplicate-instance input produces stable diagnostic severity,
  category, code, message, and metadata.

## Boundary Evidence

`scripts/check-boundaries` remains the SDE boundary gate. It scans the SDE tree
for forbidden SagaEngine, SagaEditor, SagaServer, SagaRuntime, repo-local engine
path, Forge/Prism, and Saga target references.

The Phase 19 changes do not add SDE Core dependencies on SagaRuntime,
SagaEditor, SagaServer, SagaScript, Visual Blocks, package/distribution,
StarterArena gameplay, or CSharpScriptHost.

## Changed Files

See `changed_files.txt`.

## Verification Commands

See `commands.log`.

## Command Results

The recorded local checks passed in this batch. The phase remains
`Implemented-Unverified` because maintainer verification has not occurred.

## Required Files

- `EVIDENCE.md`: present
- `commands.log`: present
- `changed_files.txt`: present
- `known_limitations.md`: present
- `verification_result.json`: present

## Manual Checks

- [x] Public docs do not overclaim.
- [x] Known limitations are documented.
- [x] Current artifact and manifest outputs are documented from real SDE code.
- [x] Deterministic artifact/manifest tests are present.
- [x] SDE boundary tooling was checked.
- [x] No phase is marked `Verified`.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

The SDE artifact/manifest contract is documented, focused SDE tests prove stable
manifest/hash/graph output and stable duplicate-instance diagnostics for
equivalent inputs, and the standalone SDE package/test proof and boundary gate
pass. Maintainer verification, engine distribution proof, new runtime/editor/
server behavior, SagaScript hosting, Visual Blocks implementation, package/
distribution ownership, StarterArena gameplay, and CSharpScriptHost changes
remain absent.
