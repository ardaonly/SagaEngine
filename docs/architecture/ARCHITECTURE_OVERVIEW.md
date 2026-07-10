# Saga Architecture Overview

> Status: Current architecture summary
> Scope: System layers, ownership, dependency direction, artifact flow, and
> evidence boundaries

This document describes SagaEngine architecture. It does not describe product
features, genre promises, editor experience goals, or sample-game output.

## System Layers

SagaEngine is organized around ownership boundaries:

```txt
Saga product layer
  coordinates project lifecycle, modes, preview, build, package, publish, and
  diagnostics entry points.

Editor layer
  owns authoring user interface and editor-side views.

Runtime layer
  owns client/game execution, package consumption, asset access, prediction,
  interpolation, and runtime services.

Server layer
  owns authoritative multiplayer execution, command validation, session policy,
  authoritative state mutation, and replication output.

Engine core
  owns reusable runtime-neutral contracts, math, ECS/simulation primitives,
  platform/backend interfaces, asset/package vocabulary, and low-level graphics
  vocabulary.

Tools
  validate, compile, analyze, package, and report through command or service
  boundaries.

Diagnostics and tests
  record focused evidence about subsystem behavior. They do not turn focused
  proof into product readiness.
```

## Ownership Rules

Engine core should stay reusable and runtime-neutral. It may expose stable
contracts and small shared data types, but implementation managers, concrete
backends, server policies, editor UI, and tool internals should not become
installed engine API by default.

Runtime consumes project/package artifacts and engine contracts. It should not
own editor authoring, asset import/cook, server authority, build tooling, or
compiler/tool internals.

Server owns authoritative simulation decisions. Clients may request, predict,
render, and interpolate; server state remains the multiplayer authority.

Editor owns authoring presentation and commands. It may display diagnostics,
invoke tools through boundaries, and edit source-controlled project resources,
but it does not own runtime truth, server truth, compiler internals, or package
truth.

Tools own offline or build-time work. They should communicate through declared
inputs, reports, manifests, artifacts, and exit status rather than linking
through private implementation details.

## Artifact Flow

The intended flow is:

```txt
Source-controlled project inputs
  -> validation and compile/analyze tools
  -> generated artifacts, manifests, and reports
  -> package/build outputs
  -> Runtime and Server consumption
  -> diagnostics and focused test evidence
```

Generated artifacts and reports support the system, but they do not replace
source-controlled truth unless a current architecture document explicitly says
so.

## Dependency Direction

Dependencies should point toward stable contracts and artifacts:

- Product may coordinate public module/service boundaries.
- Editor may consume shared contracts, tool reports, runtime preview boundaries,
  and collaboration services.
- Runtime and Server may consume engine/shared contracts and packaged artifacts.
- Tools may read source inputs and write artifacts/reports.
- Shared contracts must not depend upward into Product, Editor, Runtime, Server,
  or tool implementations.

Wrong-direction dependencies should be treated as architecture debt even when
they are temporarily allowed for legacy reasons.

Current Product, Runtime/App, Sandbox, and test graph boundary drift is tracked
in [Architecture Boundary Hardening Plan](ARCHITECTURE_BOUNDARY_HARDENING_PLAN.md).
That plan is report-only; it does not claim those boundaries are already clean.

## Evidence Boundary

Focused tests, local reports, smoke checks, and candidate reports prove only the
declared slice. They do not prove full product readiness, production networking,
full editor workflow, full asset pipeline, full SDK status, or release status.

Use the current product docs for product claims. Use this document for layer and
ownership structure.
