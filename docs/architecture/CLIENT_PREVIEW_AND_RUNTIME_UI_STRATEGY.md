# Client Preview and Runtime UI Strategy

Status: Policy checkpoint before later Hedef 2 launch and client preview work.

This document defines the boundary for alpha client preview and runtime UI
overlay work. It does not change `ClientHost`, networking, server behavior, or
runtime UI.

## Client Preview Decision

The existing `ClientHost` is blocked for alpha client preview claims until a
bounded report seam exists.

The repository can continue to use server-headless and local tool evidence, but
small-team alpha client preview must not depend on an unbounded or hard-to-exit
client path.

## Candidate Seam

A future `RuntimeClientHost` or equivalent seam may be introduced if needed. It
must be bounded, local, and report-oriented before it supports an alpha preview
claim.

Required seam properties:

- local server/client preview only;
- clean auto-exit;
- explicit timeout;
- deterministic report path;
- diagnostics for launch, connection, timeout, and shutdown;
- no real transport stress claim;
- no production network claim;
- no long-running soak behavior.

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

## Phase Boundary

Later launch phases may introduce the bounded preview seam. They must not
rewrite or migrate `ClientHost` as a side effect of this policy checkpoint.
