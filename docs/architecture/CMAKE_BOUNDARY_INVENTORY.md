# CMake Boundary Inventory

> Last updated: 2026-07-13
> Status: Report-only architecture inventory
> Scope: CMake target/interface risks, include-root risks, recursive glob risks,
> and obvious public header boundary risks.

This document records boundary risks in the current build graph. It is not an
enforcement gate and does not change build behavior.

## Classification Key

- `report-only legacy violation`: Known existing shape that should remain
  non-blocking until a focused migration owns it.
- `candidate for PRIVATE conversion`: A current public link/include/definition
  may be broader than the public ABI requires.
- `candidate for public API cleanup`: A public header or exported include root
  exposes implementation, tool, or UI details.
- `candidate for test graph narrowing`: A test target can see or link more
  modules than its subsystem should require.
- `candidate for include-root restriction`: An exported include directory makes
  another layer visible too broadly.
- `uncertain, needs inspection`: Current evidence is not enough to prescribe the
  fix safely.

## Known Risks

| ID | Classification | Evidence | Boundary risk |
|---|---|---|---|
| CBI-001 | report-only legacy violation | `cmake/modules/SagaTargets.cmake` uses recursive source collection for major targets. | Misplaced files can silently enter a target without explicit ownership review. |
| CBI-002 | report-only legacy violation | `cmake/modules/SagaTests.cmake` collects broad test source sets. | Tests can join broad executables and inherit too much include/link access. |
| CBI-003 | candidate for include-root restriction | `SagaEngine` exports both `Engine/Public` and `Runtime/include`. | Engine consumers can see Runtime headers through the Engine target interface. |
| CBI-004 | candidate for public API cleanup | `SagaEditorLib` publicly links Qt and public Editor headers expose Qt-shaped types. | Qt remains part of the Editor public ABI until those headers are redesigned. |
| CBI-005 | candidate for PRIVATE conversion | `SagaProductLib` republishes Engine, Runtime, Server, Collaboration, and Qt. | Product orchestration can become a transitive dependency hub. |
| CBI-007 | uncertain, needs inspection | `SagaBackend` exports a mixed backend include root. | Consumers can include generic backend service paths without clear ownership. |
| CBI-008 | candidate for test graph narrowing | Architecture and product tests inherit broad include roots. | Include-boundary tests are weaker when the test target can already see most modules. |

## Retired Boundaries

- CBI-006 is resolved: Runtime owns its app-local host, and the legacy client
  executable, source directory, output identity, install surface, and package
  surface are absent. Architecture tests enforce this boundary.

## Guard Model

Architecture tests should hard-fail only for boundaries that have already been
migrated or are explicitly declared as new-code rules. Legacy risks stay visible
as report-only inventory until their owner area is cleaned. This document is
not evidence that any listed cleanup is complete.

Guard candidates and enforced cleaned boundaries:

- cleaned Editor public headers must not include Qt headers or expose Qt types;
- Runtime public headers must not include Apps, Editor, Server private
  authority, or tool/compiler internals;
- Runtime, Product Shell, and Editor targets must not compile or include the
  retired client app, and the retired executable must remain absent from build,
  install, and distribution rules;
- Server public headers must not include Editor, Product shell, tool/compiler
  internals, or Qt UI;
- Engine public headers must not include Server, Editor, Apps, Product, or
  tool/compiler internals;
- new or cleaned targets must not add public dependencies without public-header
  evidence.

## Non-Claims

This inventory does not prove full test health, full public API cleanup, Qt
removal, a generic connected-client runtime, Server authority cleanup, or
explicit CMake source ownership. It records the current risks so future changes
can be scoped without turning legacy debt into accidental product claims.
