# Client Preview and Runtime UI Strategy

Status: Architecture boundary for bounded local preview and runtime UI seams.

This document defines the boundary for alpha client preview and runtime UI
overlay work. It does not add networking, server behavior, or runtime UI.

## Client Preview Decision

The legacy standalone client host has been retired. No legacy client executable
is shipped, and alpha client preview remains blocked until a bounded Runtime-
owned report seam exists.

The repository can continue to use server-headless and local tool evidence, but
small-team alpha client preview must not depend on an unbounded or hard-to-exit
client path.

## Runtime Read Seam

A future Runtime preview seam may be introduced only as a bounded, local,
report-oriented adapter over runtime-readable project state. The seam must not
become a second project model, a live editor back door, or an implicit network
stack.

Runtime-readable inputs are contract-only until implemented:

- source-controlled scene and entity truth declared by the project manifest;
- accepted asset references from source-truth gate evidence;
- non-canonical generated projections only as evidence;
- freshness evidence proving whether generated projections are fresh, partial,
  missing, stale, or invalid.

`ReadyForImplementationPlanning` style report statuses, where present in tools,
must not be read as proof that a complete Runtime or Client Preview exists.

Required seam properties:

- local server/client preview only;
- clean auto-exit;
- explicit timeout;
- deterministic report path;
- diagnostics for launch, connection, timeout, and shutdown;
- immutable handoff data copied from accepted scene/entity/project inputs;
- explicit accepted asset references;
- explicit generated-artifact freshness status;
- no real transport stress claim;
- no production network claim;
- no long-running soak behavior.

## Prerequisite Layer

Client preview depends on reportable prerequisites rather than optimistic launch
attempts:

- source-truth scene and entity validation;
- accepted asset references;
- package/launch profile alignment;
- generated projection freshness;
- local diagnostics envelope;
- no-network mode labels when networking is not part of the proof.

Missing or stale prerequisites must produce diagnostics and a no-submit result
instead of falling through to a partial preview claim.

## Adapter Audit Contract

A report-only adapter audit may read source-truth inventory, scene/entity
validation, asset reference gates, generated freshness gates, runtime readiness
gates, package alignment, and launch alignment evidence. Missing required
evidence maps to `MissingEvidence`; failed or blocked required evidence maps to
`Blocked`; partial evidence maps to `PartiallyProven`; complete evidence may
map to `ReadyForImplementationPlanning`.

The audit records missing adapter seams for scene source truth, entity source
truth, component metadata, accepted asset references, generated projection
evidence, and freshness evidence. It must not touch Runtime code.

## Preview Target

The target for later alpha work is a local bounded preview that proves a small
workflow can launch, observe status, and exit cleanly. It is not a scale,
security, or transport resilience exercise.

Generated preview reports should include:

- `schemaVersion`;
- `tool`;
- `command`;
- `status`;
- launch inputs;
- process or host lifecycle diagnostics;
- timeout configuration;
- report paths;
- evidence for clean exit or explicit failure.

## Runtime UI Overlay Scope

Runtime UI overlay scope is limited to status and report visibility:

- preview status;
- connection or host state;
- diagnostics summary;
- report path;
- exit reason.

It is not a full game UI, HUD framework, menu system, live debugger, or editor
panel replacement.

## Boundary

Preview work must not restore the retired client application as a side effect
of this strategy. The strategy defines the handoff shape and diagnostics
contract only.
