# Phase 8B Ownership Alignment Pass

> Last updated: 2026-05-26
> Status: Phase 8B docs/report-only ownership alignment
> Phase 8: Documentation / Code Alignment Enforcement

Phase 8B compares high-risk ownership claims against the dependency graph,
known boundary inventory, current source layout, and accepted architecture
checkpoints. It does not change source, CMake, test registration, guards, or
publish enforcement.

Full CTest remains unverified.

## Alignment Sources

Reviewed sources:

- `docs/DependencyGraph.md`
- `docs/architecture/CMAKE_BOUNDARY_INVENTORY.md`
- Phase 3, 4, 5, 6, and 7 closure checkpoints
- Phase 8A claim inventory
- current CMake target/include/test boundary inventory

## Ownership Matrix

| Ownership claim | Classification | Evidence | Remaining debt |
|---|---|---|---|
| Runtime startup/session/lifecycle ownership | partial | Phase 3 closure and Phase 3C checkpoint record Runtime-owned startup preflight/session/diagnostics and lifecycle registry contracts. | `ClientHost`, `ClientNetworkSession`, real Runtime service adapters, and `SagaRuntime` physical source reuse remain unresolved. |
| Runtime package asset bootstrap/access | partial | Phase 5J-5N add Runtime bootstrap diagnostics and read-only asset catalog contracts. | Durable RuntimeServiceRegistry asset service and ClientHost/runtime asset consumption remain deferred. |
| Server authoritative movement ownership | partial | Phase 4 closure records normalized input intake, actor ownership, tick-gated mutation, rejection behavior, and dirty replication intent. | `ReplicationManager` consumption, accepted-state snapshots, client reconciliation, and full multiplayer loop remain deferred. |
| AssetPipeline manifest/package ownership | partial | Phase 5 closure records identity, asset, and package manifest writers plus generated RuntimeSmoke package proof. | Source discovery/import/cook and UI/document asset workflow remain deferred. |
| Product publish readiness ownership | partial | Phase 5O records product-local package/asset/identity coverage reporting. | Publish hard gates and Runtime startup parity remain deferred to later publish work. |
| Editor public Qt boundary ownership | partial | Phase 6R records migrated panel/API surfaces and a passing public Qt ABI guard. | `GraphCanvas.h`, `QtPanel.h`, and `SagaEditorLib` public Qt visibility remain explicit debt. |
| Editor project/workspace ownership | partial | Phase 7B maps product/editor/lab ownership and Phase 7C implements editor-local `ProjectManager`. | Product shell open flow, dashboard project rows, and project browser real workspace behavior remain deferred. |
| CMake source and target ownership | deferred debt | CMake boundary inventory records recursive globs, broad public links, and broad include roots as report-only legacy risks. | Explicit source lists, narrowed public links, and broader hard-fail target checks require later owner slices. |
| Test ownership evidence | deferred debt | CMake boundary inventory records broad test include roots and `SagaUnitTests` as coarse evidence. | Test target narrowing and suite-level proof remain later work. |
| Collaboration truth ownership | future intent | Dependency graph assigns collaboration truth to `SagaCollaboration`; Phase 7 keeps editor collaboration product readiness unclaimed. | Collaboration workflows, permissions, locks, conflict resolution, and publish-ready team state remain deferred. |

## Alignment Findings

Supported alignment:

- Recovery closure docs consistently use foundation/checkpoint language for
  Phases 3 through 7.
- The dependency graph and closure docs agree that product/editor/runtime/server
  ownership must stay separated.
- Known CMake/public-header risks are already classified as report-only or
  deferred ownership work.

Partial alignment:

- Runtime, server, asset/package, publish, editor Qt, and editor project claims
  are supported only at bounded foundation level.
- Test evidence exists for focused slices, but broad test include/link roots
  weaken ownership proof.

Stale or correction-needed:

- No recovery-roadmap ownership claim requires immediate correction in Phase
  8B.
- Non-recovery roadmap wording with broad `Qt-free`, `production-ready`,
  `server-authoritative`, or `publish-ready` language remains deferred to
  evidence-specific correction slices.

## Non-Goals

Phase 8B does not:

- edit source or CMake
- change test registration or labels
- add or harden architecture guards
- change `docs/DependencyGraph.md`
- implement product/runtime/editor behavior
- enforce publish readiness
- rewrite non-recovery roadmaps
- prove full CTest health

## Verification

Verification completed for this report:

- `git diff --check`
- touched-doc trailing whitespace check
- targeted `rg` for Phase 8B wording, ownership claims, classifications,
  stale claims, non-goals, and full CTest caveats
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Full CTest remains unverified.
