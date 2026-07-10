# Saga Dependency Graph

> Purpose: Current dependency and ownership map for Saga product, editor,
> runtime, server, shared contracts, collaboration, Forge, Prism, asset
> pipeline, scripting, and tools.

This document describes current dependency direction. It is not a roadmap,
product-readiness claim, or replacement for source code and tests.

## Core Rule

Dependencies should point toward stable contracts, public APIs, reports,
manifests, and generated artifacts. They should not point upward into product
shells, UI implementations, private runtime/server systems, or tool internals.

## Intended Direction

```txt
Product shell
  -> public app/module boundaries
  -> tool commands and reports

Editor
  -> editor public contracts
  -> shared contracts
  -> tool reports
  -> runtime preview boundaries

Runtime
  -> engine public contracts
  -> shared contracts
  -> package/runtime artifacts

Server
  -> engine public contracts
  -> shared contracts
  -> authority/session contracts

Tools
  -> source-controlled inputs
  -> reports, manifests, packages, generated artifacts

Shared contracts
  -> no upward dependency into product, editor, runtime, server, app, or tool
     implementation details
```

## Ownership Table

| Area | Owns | Must not own |
|---|---|---|
| Product shell | Project lifecycle, mode routing, report-first workflow orchestration | Editor implementation, runtime execution, server authority, compiler/tool internals |
| Editor | Authoring UI, panels, diagnostics display, editor commands | Product lifecycle, runtime/server truth, package truth, tool internals |
| Runtime | Client/game execution, package consumption, asset access, runtime services | Editor UI, asset import/cook, build pipeline internals |
| Server | Authoritative simulation, command validation, sessions, replication output | Editor UI, product shell, tool internals |
| Engine core | Runtime-neutral contracts, math, ECS/simulation, platform/graphics contracts | Product/editor/server/app ownership |
| Shared contracts | Stable ids, manifests, diagnostics, package and scripting vocabulary | Implementations or upward includes |
| Forge | Build and repository orchestration | Product shell behavior, runtime/editor/server internals, implementation ownership for other tools |
| Prism | Repository-analysis tooling when used explicitly | Build ownership, source mutation, runtime/editor/server ownership |
| Asset pipeline | Asset import/cook/report ownership | Runtime execution or editor UI ownership |
| SagaScript | C# analysis, manifest/artifact emission, selected compile/toolchain work | Runtime/server execution or editor UI ownership |

## Boundary Rules

- Product code may launch tools and read reports; it must not become the tool
  implementation.
- Runtime and server code consume validated package/runtime artifacts; they must
  not import editor or product shell implementation.
- Editor code may present diagnostics and authoring views; it must not become
  runtime/server/package truth.
- Shared public headers must stay small and implementation-neutral.
- New public dependencies require public-header evidence and focused tests.

## Evidence

Current boundary evidence is tracked through focused unit tests, architecture
tests, CMake inventories, and local reports. Passing one focused boundary check
does not prove full graph health.
