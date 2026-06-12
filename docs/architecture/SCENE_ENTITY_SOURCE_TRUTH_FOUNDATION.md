# Scene / Entity Source Truth Foundation

## Status

This document defines the source-truth boundary for scene and entity data. It is
documentation and report-only evidence; it does not add runtime, server,
ClientHost, editor UI, Qt UI, asset import, or asset cook behavior.

## Source Truth Definitions

A Scene is a source-controlled project resource that owns a stable scene id,
scene metadata, entity membership, scene-level references, generated artifact
links, and read-boundary metadata.

An Entity is a source-controlled record inside a Scene. It owns a stable entity
id, display metadata, component metadata, and declared asset references. It is
not proof of runtime spawn/despawn, replication, gameplay authority, or ECS
completion.

Scene and Entity files are canonical only when they are referenced by the
project manifest and checked in as source-controlled data. Generated reports,
package manifests, projections, source maps, diagnostics, and build outputs are
evidence snapshots. They are never canonical authoring source.

## Asset Reference Ownership

Project manifests own asset roots. Scene and Entity source files own usage
references to those assets. Generated artifacts may summarize or validate asset
references, but they do not become the owner of an asset reference.

Every accepted Scene or Entity asset reference must declare an owner and a
project-relative path or known project asset id. Missing ownership is reported
as source truth debt.

## Runtime Read Prerequisites

Future runtime-readable inputs are accepted source-controlled scene/entity
records, accepted asset references, and generated projections only when their
freshness evidence is proven or explicitly partial. Generated artifacts,
package summaries, launch summaries, editor diagnostics, stale projections, and
asset import/cook outputs must not become canonical runtime source truth.

Runtime read behavior remains report-only here. This document does not wire
Runtime gameplay, Server gameplay, ClientHost, asset consumption, or preview
launch behavior.

## Editor Read Boundaries

Editor inspection is report-only in this block. Later Editor work may inspect
source truth and generated evidence, but this block does not add scene editing,
entity placement, Editor UI, Qt UI, or public Qt ABI expansion.

Client Preview remains deferred until a bounded preview seam is defined and
proven through deterministic reports.

## Residual Non-Claims

- No Product Beta status.
- No Release Candidate status.
- No Production Readiness status.
- No Playable Editor status.
- No Client Preview or ClientHost implementation.
- No Runtime gameplay or Server gameplay implementation.
- No asset import or asset cook implementation.
- No raw full CTest, heavy stress, soak, bot swarm, or real transport proof.
