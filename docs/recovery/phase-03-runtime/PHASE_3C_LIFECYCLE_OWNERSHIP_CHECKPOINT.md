# Phase 3C Lifecycle Ownership Checkpoint

> Last updated: 2026-05-25
> Status: Historical Phase 3C checkpoint; superseded by Phase 3 closure checkpoint
> Decision: Phase 3C-4 was completed; see `PHASE_3_CLOSURE_AND_PHASE_4A_OPENING_CHECKPOINT.md`

Superseding checkpoint:
`docs/recovery/phase-03-runtime/PHASE_3_CLOSURE_AND_PHASE_4A_OPENING_CHECKPOINT.md`.

This checkpoint records what Phase 3C proves, what it does not prove, and the
minimum criteria for moving from Runtime ownership realignment into the Server
Authoritative Movement phase.

Full test health remains unverified.

## Phase 3C Lifecycle Foundation Evidence

Phase 3C is acceptable as a Runtime lifecycle foundation package, not as a
complete Runtime ownership layer.

- Phase 3C-1 established Runtime-owned service lifecycle contracts: service descriptors, service states, lifecycle diagnostic severity, lifecycle diagnostics, lifecycle results, and transition helpers.
- Phase 3C-2 added `RuntimeServiceRegistry`: `IRuntimeService`, deterministic registration order, start/tick/stop orchestration, state queries, duplicate/invalid descriptor validation, aggregate reports, and failed-start blocking.
- Phase 3C-3 added `RuntimeServiceRegistryDiagnostics`: `Idle` / `Ready` / `Blocked` classification, operation success, service result counts, diagnostic counts, and diagnostic views preserving severity, service id, and message.
- Focused `unit;runtime` targets now exist for startup preflight/session/diagnostics and service lifecycle/registry/registry diagnostics.
- Runtime boundary remains clean of `Apps/Client`, `ClientHost`, `ClientConfig`, and `Saga::ClientConfig` references in `Runtime/include` and `Runtime/src`.
- Prior implementation verification included successful `SagaRuntimeLib` build, successful `RuntimeServiceRegistryDiagnosticsTests` build, and passing `ctest -R RuntimeServiceRegistryDiagnosticsTests`.

## Current Runtime Maturity

| Area | Rating | Evidence |
|---|---|---|
| startup ownership | `startup-owned` | Runtime owns startup preflight facade and package validation handoff. |
| launch/session intent | `startup-owned` | `RuntimeLaunchOptions`, `RuntimeSessionDescriptor`, and `RuntimeStartupSession::Prepare` exist in Runtime. |
| startup diagnostics | `startup-owned` | `RuntimeStartupDiagnostics` classifies startup reports; Apps/Runtime owns logging and exit policy. |
| service lifecycle contracts | `registry-ready` | Generic service descriptors, states, diagnostics, results, and transition semantics exist. |
| service registry | `registry-ready` | Registry owns registration validation, order, state, and lifecycle operation reports. |
| lifecycle diagnostics | `registry-ready` | Registry reports have Runtime-owned summary/view classification. |
| Apps/Runtime integration | `startup-owned` for startup, `registry-shell` for lifecycle registry | Phase 3C-4 made Apps/Runtime create and consume `RuntimeServiceRegistry` through an app-local bootstrap marker service. |
| real service adapters | `symbolic` | No Runtime-owned network/input/UI/render/asset/world services exist. |
| ClientHost decomposition | `symbolic` | `ClientHost` and inline `ClientNetworkSession` remain app/client-owned. |
| overall Runtime layer | `registry-ready` | Runtime has startup and lifecycle foundations plus a narrow app-facing lifecycle registry shell, but no real service adapters. |

## What Runtime Can Claim Now

Runtime can claim:

- startup-facing ownership is established for preflight, normalized launch/session intent, and startup diagnostics
- lifecycle contract and registry foundation exists
- lifecycle report classification exists
- Runtime can be tested without `Apps/Client`

Runtime cannot yet claim:

- a complete runtime layer
- real service ownership
- `ClientHost` ownership
- network/session/UI/rendering/asset/world lifecycle ownership

## Apps/Runtime Integration Readiness

Phase 3C-4 Apps/Runtime Registry Integration Shell was completed after this
checkpoint.

Apps/Runtime now safely:

- create a `SagaRuntime::RuntimeServiceRegistry` during startup flow
- register a small app-local bootstrap/lifecycle adapter if useful
- use `RuntimeServiceRegistryDiagnostics` to summarize lifecycle operation reports
- keep log wording and process exit policy local to `Apps/Runtime`
- keep `Saga::ClientHost` construction local and unchanged

Premature work for Phase 3C-4:

- moving `ClientHost`
- splitting `ClientNetworkSession`
- creating real network/UI/render/asset/world services
- making Runtime parse CLI arguments or own process exit policy
- moving `Saga::ClientConfig` or app config conversion into Runtime

## Recommended Phase 3C-4 Shape

Implemented as a narrow Apps/Runtime registry shell.

- Integration point: after successful `RuntimeStartupSession::Prepare` and before `Saga::ClientHost` construction.
- API usage: include `RuntimeServiceRegistry.hpp` and `RuntimeServiceRegistryDiagnostics.hpp`; create a local `RuntimeServiceRegistry`; start a tiny app-local lifecycle adapter or bootstrap marker service; classify the report with `RuntimeServiceRegistryDiagnostics`.
- Ownership rule: the adapter must live in `Apps/Runtime` or an app-local translation unit, not Runtime.
- Runtime API rule: no new Runtime APIs are expected for this slice.
- Test shape: use existing Runtime-only tests plus app build verification; add an app-local focused test only if an existing low-cost pattern is available.
- Boundary verification: keep the forbidden Runtime dependency search as a required check.
- `ClientHost` rule: construct and own `ClientHost` exactly where it is today.

No real network, input, UI, rendering, asset, or world services should be
introduced in Phase 3C-4.

## Phase 3 Closure Criteria

Minimum criteria before Phase 4:

- Phase 3 startup ownership checkpoint is accepted.
- Phase 3C lifecycle foundation checkpoint is accepted.
- Apps/Runtime registry integration shell exists, or is explicitly deferred with a documented reason.
- Runtime public headers and implementation remain free of app/client types.
- `ClientHost` remaining in `Apps/Client` is documented as unresolved Phase 3 debt.
- `SagaRuntime` source reuse of `Apps/Client/ClientHost.h/.cpp` is documented as unresolved debt.
- Runtime test inventory is current.
- Full test health remains explicitly unverified unless the relevant full suite set is actually run.

## Phase 4 Readiness Decision

Phase 4A may start after Phase 3C-4 only as a report-only Server Authoritative
Movement Audit.

The superseding closure checkpoint records that:

- Runtime startup/lifecycle foundation is checkpointed
- Apps/Runtime consumes the registry in a narrow shell
- remaining `ClientHost`, `ClientNetworkSession`, and `SagaRuntime` source reuse debt is documented rather than hidden
- Phase 3 is not closed as complete Runtime/App ownership

## Phase 4 Opening Scope

Phase 4A opens with a Server Authoritative Movement Audit.

It should not start as:

- a broad networking rewrite
- a full MMO gameplay system
- a mass move of server/client files

The first Phase 4 audit should inspect:

- current server authority model
- current client command path
- current replication/prediction assumptions
- current server/session ownership
- minimum authoritative gameplay loop to prove
- what `MultiplayerSandbox` will eventually need from server authority

## Verification Summary

Commands run for this checkpoint:

- `git status --short`
- `git diff --check`
- `git diff --name-only`
- `git diff --stat`
- `rg -n "RuntimeServiceLifecycle|RuntimeServiceRegistry|RuntimeServiceRegistryDiagnostics|IRuntimeService|RuntimeServiceRegistryReport|RuntimeStartupPreflight|RuntimeStartupSession|RuntimeStartupDiagnostics|ClientHost|ClientNetworkSession|Phase 3C" Runtime Apps/Runtime Apps/Client Tests/Unit/Runtime cmake/modules docs/dev docs/roadmaps`
- `rg -n "Apps/Client|ClientHost|ClientConfig|Saga::ClientConfig" Runtime/include Runtime/src`
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L runtime`

Results:

- `git diff --check` passed.
- Runtime forbidden dependency search returned no matches.
- Runtime label inventory listed 12 runtime-labeled tests, including `RuntimeServiceLifecycleTests`, `RuntimeServiceRegistryTests`, and `RuntimeServiceRegistryDiagnosticsTests`.
- The worktree is broadly dirty with unrelated tracked and untracked changes.
- Architecture suite was not run.
- Full test health remains unverified.

## Decision Gate

Phase 3 is closed only as Runtime startup/lifecycle foundation established.

Phase 4A is open only as a report-only Server Authoritative Movement Audit.

Do not treat the Phase 3C-4 registry shell as real network/UI/render/asset/world
service ownership, and do not start authoritative movement implementation
without a separate post-audit approval.
