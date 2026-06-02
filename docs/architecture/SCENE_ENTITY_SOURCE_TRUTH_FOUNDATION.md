# Scene / Entity Source Truth Foundation

## Status

Phase 127-129 contract for the Hedef 4 opening block. The contract is
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

## Read Boundaries

Runtime read behavior is report-only in this block. Later Runtime work may read
accepted source truth or fresh generated projections, but this block does not
wire Runtime gameplay, Server gameplay, ClientHost, asset consumption, or
preview launch behavior.

Editor inspection is report-only in this block. Later Editor work may inspect
source truth and generated evidence, but this block does not add scene editing,
entity placement, Editor UI, Qt UI, or public Qt ABI expansion.

Client Preview remains deferred until a later phase defines and proves a bounded
preview seam.
