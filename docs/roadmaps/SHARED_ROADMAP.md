# SagaEngine — Shared Editor / Runtime Roadmap

> Last updated: 2026-04-25
> Target: Subsystems that span the editor binary and the runtime engine,
> and therefore cannot live cleanly in either `ROADMAP.md` (engine
> core) or `EDITOR_ROADMAP.md` (authoring tools).

This document captures the layers where the editor and the runtime
share a contract — the C# scripting host, the canonical IR and
compilation pipeline, the package SDK manifest format, the security
boundary policy, and the cross-cutting profiling / replay tooling that
emits signals from runtime code and consumes them in editor tools.
Items here always have at least one editor consumer and at least one
runtime consumer; pure runtime items belong in `ROADMAP.md` and pure
authoring items belong in `EDITOR_ROADMAP.md`.

Conventions used throughout match the other two roadmaps: `[x]` means
shipped, `[ ]` means open, and the note after a shipped item names the
files that represent the work.

---

## 1. C# Scripting Runtime (.NET 8)

The runtime hosts CoreCLR; the editor consumes the same hosting layer
to run preview scripts, evaluate inspector expressions, and drive the
visual scripting compiler. Anything that is purely managed-code-only or
purely runtime-only stays in the engine roadmap; everything below has
to behave the same way in the editor and in the shipped game.

| Status | Item |
|--------|------|
| [ ] | CoreCLR host integration — runtime init, shutdown, error propagation, and deterministic teardown. |
| [ ] | Collectible `AssemblyLoadContext` lifecycle — per-package load, unload, dependency resolution, and stale-reference detection. |
| [ ] | Native → Managed bridge — engine events, tick callbacks, and object handles exposed to C#. |
| [ ] | Managed → Native bridge — generated, type-safe calls with no reflection on hot paths. |
| [ ] | Canonical gameplay API — `IEntity`, `IWorld`, `IComponent`, `IQuery`, `ISystem`. |
| [ ] | Safe marshalling layer — blittable structs, strings, spans, arrays, handles, and ownership rules. |
| [ ] | Thread-safe call boundary — async-safe dispatch between engine threads and script threads. |
| [ ] | Cooperative execution budget — per-tick script time budget, watchdog, cancellation, and safe fallback. |
| [ ] | Script hot reload — supported edit sets reload in place; unsupported edits fall back to rebuild / restart. |
| [ ] | Script package format — manifests, dependency resolution, versioning, compatibility checks. |
| [ ] | Script diagnostics — compile errors, runtime exceptions, stack traces, source maps, profiling markers. |
| [ ] | Deterministic test harness — repeatable script replay under fixed inputs and fixed tick order. |
| [ ] | Sandbox worker mode — out-of-process isolation for untrusted scripts with file / network restrictions and crash containment. |
| [ ] | Script SDK — library authors register gameplay APIs, script APIs, and generated bindings through a documented extension surface. |

---

## 2. Canonical IR and Compilation Pipeline

The visual scripting block editor and the C# subset editor both lower
into the same intermediate representation. The IR-to-runtime compiler
is loaded by both the editor (for preview) and the runtime (for
shipped gameplay), so the contract is shared.

| Status | Item |
|--------|------|
| [ ] | Canonical IR definition — one intermediate representation shared by block authoring and text authoring. |
| [ ] | Block-to-IR compiler — graph nodes, typed pins, collapse groups, and macro expansion lower into IR. |
| [ ] | C# subset parser — supported gameplay syntax parses into the same IR. |
| [ ] | IR-to-runtime compiler — IR lowers into compiled script methods, bytecode, or another optimised runtime form. |
| [ ] | Round-trip policy — supported constructs preserve meaning across block view and text view. |
| [ ] | Unsupported syntax policy — unsupported C# features fail clearly instead of silently drifting. |
| [ ] | Metadata preservation — comments, symbols, and user-facing labels survive supported round trips where feasible. |
| [ ] | Incremental compilation — dirty-region rebuild for both blocks and text. |
| [ ] | Background compile workers — parse, validate, and regenerate without blocking editor input. |
| [ ] | Generated node metadata — source generators emit node descriptors, docs, and validation hooks. |

---

## 3. Performance Model and Runtime Boundaries

These rules constrain what the editor is allowed to do at edit time
and what the runtime is allowed to do at simulation time. They are
shared because every rule has both an editor side ("blocks regenerate
C# off the hot path") and a runtime side ("gameplay never interprets
the graph node-by-node").

| Status | Item |
|--------|------|
| [ ] | Authoring / runtime separation — blocks and C# are editor-facing representations, not runtime interpreters. |
| [ ] | Runtime execution rule — gameplay runs from compiled output, never from node-by-node graph interpretation on the main simulation path. |
| [ ] | Edit-time sync cost containment — block movement, parse updates, and C# regeneration happen off the hot path. |
| [ ] | Hot-reload debounce — changes batch before rebuild to avoid thrashing on every keystroke. |
| [ ] | Partial recompilation — only changed compilation units regenerate when possible. |
| [ ] | Reflection avoidance — runtime invocation uses generated bindings, cached delegates, or direct dispatch. |
| [ ] | Allocation control — pooled objects, stack-friendly value types, and native arena-style allocation where appropriate. |
| [ ] | Hot-path separation — combat, netcode, replication, pathfinding, and tight AI loops stay native or otherwise optimised. |
| [ ] | Runtime budget enforcement — script work is capped per tick and can yield or be suspended safely. |
| [ ] | Editor-only overhead acceptance — extra cost is allowed in the editor, not in shipped simulation. |

---

## 4. Package SDK (shared parts)

The package manifest schema, the version compatibility rules, and the
trust model are shared because both the runtime loader and the editor
extension host parse the same manifest. The editor-only API surface
(panels, inspectors, menus, graph nodes) lives in
`EDITOR_ROADMAP.md`; the runtime-only API surface (gameplay scripts,
generated bindings) lives in `ROADMAP.md`.

| Status | Item |
|--------|------|
| [ ] | Package manifest schema — explicit package identity, dependencies, compatibility range, and load order. |
| [ ] | Extension discovery — runtime and editor enumerate installed packages without hard-coded registration. |
| [ ] | Node registration API — third-party packages can add block nodes, categories, tooltips, and validation metadata. |
| [ ] | Script library API — packages expose gameplay helpers, shared types, and generated bindings. |
| [ ] | Version compatibility rules — package load fails fast on incompatible engine or scripting ABI versions. |
| [ ] | Isolation and unload rules — packages unload cleanly when no managed references remain. |
| [ ] | Sample packages — reference packages for gameplay scripts, block nodes, editor tools, and asset pipeline extensions. |
| [ ] | Package signing / trust model — trusted, local, and untrusted packages follow different load policies. |

---

## 5. Debugging, Profiling, and Validation

The runtime emits signals (counters, traces, snapshots); the editor
consumes them (graph debugger, replay viewer, state diff). Replay
verification straddles the boundary because the captured trace runs
in either host.

| Status | Item |
|--------|------|
| [ ] | Script profiler — per-method and per-node timing for C# and block-authored gameplay. |
| [ ] | Graph debugger — node-level breakpoints, watch values, and execution tracing. |
| [ ] | Runtime trace capture — inputs, replication, script events, and simulation state snapshots. |
| [ ] | State diff tooling — compare world state, snapshot state, and replay state across runs. |
| [ ] | Failure reproduction runner — load a captured scenario and replay it locally or in CI. |
| [ ] | Performance budgets — editor latency, script time, compile time, and frame budgets are enforced and reported. |
| [ ] | Memory and handle leak detection — engine-owned allocations, managed bridges, and streaming resources are tracked. |
| [ ] | Replay verification — deterministic replay checks run on changes to simulation, ECS, or networking code. |
| [ ] | Crash artifact capture — logs, traces, snapshots, and minimal repro data uploaded for failures. |

---

## 6. Security and Isolation Policy

The trust model applies equally to a script running in a shipped game
and to a package loaded by the editor. The boundary is the same; the
defaults differ.

| Status | Item |
|--------|------|
| [ ] | Trusted in-process mode — first-party packages and trusted gameplay scripts may run inside the main process. |
| [ ] | Untrusted out-of-process mode — sandboxed scripts run in separate worker processes with restricted access. |
| [ ] | Permission policy — file, network, reflection, and native access controlled by package trust level. |
| [ ] | Failure containment — script crashes, deadlocks, and runaway allocations do not take down the editor or server. |
| [ ] | Security boundary documentation — clear rules for what the engine guarantees and does not guarantee. |
| [ ] | Host fallback policy — unsupported or dangerous script features fail into safe fallback paths. |

---

## 7. Documentation and Sample Content

Reference content is shared because every sample exercises both the
editor (authoring) and the runtime (execution). Pure-engine API docs
stay in `ROADMAP.md`; pure-editor UI docs stay in `EDITOR_ROADMAP.md`.

| Status | Item |
|--------|------|
| [ ] | Reference gameplay package — a minimal but complete gameplay implementation using C#. |
| [ ] | Reference block graph — a complete example showing block authoring, IR generation, and runtime execution. |
| [ ] | Reference editor extension — a sample custom panel or tool shipped as an external package. |
| [ ] | Reference asset pipeline — import, cook, validate, and preview flow documented end-to-end. |
| [ ] | Supported C# subset docs — exactly what can round-trip to blocks and what cannot. |
| [ ] | Migration guide — how to move from block-authored logic to text-authored logic without breaking gameplay. |

---

## Definitions

- **Shared**: at least one editor consumer and at least one runtime
  consumer. Lives here.
- **Runtime-only**: only the engine binary or the dedicated server
  links it. Lives in `ROADMAP.md`.
- **Editor-only**: only the editor binary links it. Lives in
  `EDITOR_ROADMAP.md`.

When in doubt, check the include tree: if a header is reachable from
both `Engine/include/` and `Editor/include/` paths, the item belongs
here.
