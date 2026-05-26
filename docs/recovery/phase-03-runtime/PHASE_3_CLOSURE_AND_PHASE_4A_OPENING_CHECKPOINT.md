# Phase 3 Closure And Phase 4A Opening Checkpoint

> Last updated: 2026-05-25
> Status: Phase 3 closed as Runtime startup/lifecycle foundation established
> Decision: open Phase 4A as a report-only Server Authoritative Movement Audit

This checkpoint closes Phase 3 narrowly. It does not claim complete Runtime/App
ownership, real service ownership, or server-authoritative gameplay.

Full test health remains unverified.

## Phase 3 Completion Evidence

Phase 3 can close as a Runtime startup/lifecycle foundation phase.

- Phase 3A Runtime/App audit documented Runtime/App ownership risks in the recovery roadmap and CMake boundary inventory, including `SagaRuntime` reuse of `Apps/Client/ClientHost.h/.cpp` as report-only Phase 3 debt.
- Phase 3B established Runtime-owned startup preflight, normalized launch/session intent, and startup diagnostics through `RuntimeStartupPreflight`, `RuntimeStartupSession`, and `RuntimeStartupDiagnostics`.
- Startup ownership keeps CLI parsing, logging text, process exit policy, and `Saga::ClientConfig` conversion local to `Apps/Runtime`.
- Phase 3C established Runtime-owned `RuntimeServiceLifecycle`, `RuntimeServiceRegistry`, and `RuntimeServiceRegistryDiagnostics`.
- Phase 3C-4 integrated `Apps/Runtime` with a narrow registry shell: it creates a `RuntimeServiceRegistry`, registers an app-local `RuntimeBootstrapService`, summarizes start/stop reports, and leaves `ClientHost` construction unchanged.
- Focused `unit;runtime` tests exist for startup and lifecycle registry surfaces.

## Phase 3 Closure Claim

Phase 3 may claim:

- Runtime startup ownership is established.
- Runtime lifecycle foundation is established.
- `Apps/Runtime` has a narrow registry integration shell.
- Runtime public headers and implementation remain clean of app/client types.
- Focused `unit;runtime` tests exist and passed during implementation verification.
- Phase 3 produced a usable foundation for app-facing Runtime startup/lifecycle orchestration.

Phase 3 may not claim:

- complete Runtime/App ownership
- `ClientHost` movement
- `ClientNetworkSession` split
- real network/session/UI/rendering/asset/world services owned by Runtime
- removal of `SagaRuntime` source reuse of `Apps/Client/ClientHost.h/.cpp`
- server gameplay authority
- full test health

## Remaining Phase 3 Debt

| Debt item | Current owner | Desired eventual owner | Risk | Likely future phase |
|---|---|---|---|---|
| `ClientHost` owns network/input/UI/render/world orchestration | `Apps/Client` | Split between Runtime-owned lifecycle services and app-local composition | Runtime remains partly symbolic around the real player loop | Later Phase 3 follow-up or post-Phase 4 Runtime service extraction |
| `ClientNetworkSession` is inline in `ClientHost.cpp` | `Apps/Client` | Dedicated client/session module with clear Runtime boundary | Network/session behavior is hard to test and migrate | Phase 4/Runtime integration follow-up |
| `SagaRuntime` builds `Apps/Client/ClientHost` sources | `SagaRuntime` target via CMake | Runtime executable composes stable app/runtime contracts, not app/client source reuse | Physical ownership drift remains hidden by target wiring | Phase 3 debt follow-up |
| Runtime has no real service adapters | none | Runtime-owned service adapters after real ownership boundaries are proven | Synthetic lifecycle shell may be mistaken for real ownership | Later Runtime service migration |
| App CLI/logging/exit policy are local but informal | `Apps/Runtime` | Explicit app/process policy contract if needed | Future slices may accidentally push policy into Runtime | Later app shell cleanup |
| Full Runtime test health is partial | focused test targets | Suite-level evidence when needed | Overclaiming stability | Any closure/release checkpoint |

## Phase 4A Opening Decision

Phase 4 may open now only as Phase 4A: Server Authoritative Movement Audit.

Phase 4A is report-only. It must not begin as:

- a broad server/network rewrite
- a full MMO gameplay implementation
- a mass move of server/client files
- a Runtime ownership continuation hidden inside server work

## Phase 4A Audit Scope

Phase 4A should inspect and report on:

- current server authority model
- current client command path
- current replication and prediction assumptions
- current server/session ownership
- current dependencies between Server, Engine, Apps/Client, Apps/Runtime, and Runtime
- existing tests related to server, networking, replication, commands, and sessions
- minimum authoritative gameplay loop required for a future `MultiplayerSandbox`

Concrete repo areas to inspect:

- `Server/Gameplay`: `GameplayCommandDispatcher` and stub handlers, especially `OnMoveRequest`
- `Server/Simulation`: `InputCommandQueue`, `MovementValidator`, and any command routing surface referenced by those files
- `Server/Networking/Server`: `ZoneServer`, connection/session lifecycle, fixed tick loop hooks
- `Server/Networking/Replication`: `ReplicationManager`, `ReplicationGraph`, snapshots, replication state
- `Apps/Client/ClientHost.cpp`: input command send path, prediction/reconciliation, snapshot apply path
- tests under `Tests/Unit/Gameplay`, `Tests/Unit/Networking`, `Tests/Integration`, and `Tests/Replication`

## Phase 4A Outputs

The Phase 4A audit should produce:

- server authority ownership inventory
- client-to-server command flow map
- replication/prediction boundary map
- first narrow authoritative movement implementation candidate
- test strategy for that candidate
- verification plan
- unresolved risks and explicit non-goals

The first implementation candidate should likely be narrow movement authority:

- accept a movement/input command
- validate identity and movement limits
- mutate server-owned state on fixed tick only
- reject invalid or unauthorized movement
- emit replication output only for accepted state changes

No implementation should happen in Phase 4A unless separately approved.

## Verification Summary

Commands run for the Phase 3 closure decision:

- `git status --short`
- `git diff --check`
- `git diff --name-only`
- `git diff --stat`
- Runtime/App/Client/docs inventory scans
- forbidden Runtime dependency scan
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L runtime`
- read-only inspection of Server command, movement, input queue, zone server, replication, and test surfaces

Results recorded for closure:

- Worktree is broadly dirty with unrelated tracked and untracked changes.
- `git diff --check` passed.
- Runtime forbidden dependency scan returned no matches.
- Runtime inventory listed 12 runtime-labeled tests.
- Runtime includes startup preflight/session/diagnostics and service lifecycle/registry/diagnostics.
- `Apps/Runtime` contains the Phase 3C-4 app-local registry shell.
- Architecture suite was not run.
- Full test health remains unverified.

## Decision Gate

Phase 3 is closed only as: Runtime startup/lifecycle foundation established.

Phase 4A is open only as: report-only Server Authoritative Movement Audit.

Do not revise Phase 3 before moving on unless new evidence contradicts the
Runtime boundary or Phase 3C-4 integration.
