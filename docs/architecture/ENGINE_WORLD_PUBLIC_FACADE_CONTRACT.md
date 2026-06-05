# Engine World Public Facade Contract

> Last updated: 2026-06-05

This document records the intended public `SagaEngine/World` facade boundary
after moving the remaining WorldNode/SimCell cluster private. It follows
Option B from
[Engine WorldNode / SimCell facade audit](ENGINE_WORLD_NODE_FACADE_AUDIT.md):
create a narrow World facade first, then move the WorldNode/SimCell cluster
private.

This is a design record for a transitional public facade shell, not a stable
SDK contract. The World public/private boundary is not fixed by this document.

## Current Context

The Partition/Streaming World headers have been moved private. The old
WorldNode/SimCell cluster headers have also been moved private:

- `SagaEngine/World/DomainScheduler.h`
- `SagaEngine/World/EventStream.h`
- `SagaEngine/World/RelevanceGraph.h`
- `SagaEngine/World/SimCell.h`
- `SagaEngine/World/WorldNode.h`

Before the move, `WorldNode.h` exposed cells, sessions, replication, scheduler,
event stream, relevance graph, mutable `SimCell` access, and mutable
`WorldState` access. Those details are now private implementation surface.

`SagaEngine/World/WorldFacade.h` exists as the narrow public facade shell. It
is intentionally lightweight and transitional.

## Future Public Boundary

Allowed future public facade responsibilities:

- lifecycle: initialize, shutdown, and tick the world runtime;
- configuration input through lightweight facade-owned config structs;
- high-level world statistics and status;
- minimal command submission for world-level actions;
- minimal query and read-only inspection;
- diagnostics-safe reporting through copied summaries or stable records.

Forbidden future public responsibilities:

- exposing SimCell directly;
- exposing DomainScheduler directly;
- exposing EventStream directly;
- exposing RelevanceGraph directly;
- exposing mutable WorldState directly;
- exposing client session internals;
- exposing replication/delta/snapshot internals;
- exposing dirty component tracking internals;
- exposing persistence/replay implementation details.

Public signatures should use lightweight facade-owned DTOs and stable shared
value types only. Allowed public signature inputs and outputs include:

- facade-owned config, status, stat, command, query, and result structs;
- stable scalar identifiers and counts;
- stable public math or ECS value types where a command/query needs them;
- copied diagnostics records that do not expose mutable internal state.

Public signatures must not expose these internal types:

- `SimCell`, `SimCellId`, `SimDomainType`, `SimDomainMask`,
  `SimCellEvent`, or `DomainTickDispatcher`;
- `DomainScheduler`, `EventStream`, `WorldEvent`, `WorldSnapshot`,
  `EventHandlerFn`, or persistence/replay helpers;
- `RelevanceGraph`, `RelevanceRule`, `RelevanceEdge`, or
  `RelevanceResult`;
- `ClientSession`, session acknowledgement state, or session replication
  cursors;
- `ComponentMask`, `EntityDirtyState`, snapshot builder state, or dirty
  component tracking maps;
- mutable `WorldState`, subsystem references, raw internal pointers, or mutable
  references to World internals.

## Public/Private Split

Future public API candidates:

- a single narrow facade class;
- lightweight facade-owned config/stat/status structs;
- minimal command/query/result structs if lifecycle and status are not enough
  for the first implementation slice.

Future private/internal implementation:

- `WorldNode`;
- `SimCell`;
- `DomainScheduler`;
- `EventStream`;
- `RelevanceGraph`;
- replication, session, persistence, replay, dirty tracking, and world-state
  internals.

The facade should own or reference the implementation through an opaque private
implementation boundary. Public callers should not be able to observe whether
the implementation is backed by `WorldNode`, a server wrapper, or a later
runtime owner.

## Naming Decision

| Candidate | Assessment |
| --- | --- |
| `WorldFacade` | Best fit for this recovery phase. It clearly describes a boundary wrapper and does not imply ownership, service registration, or kernel internals. |
| `WorldRuntime` | Reasonable if the type eventually owns runtime execution, but too strong before ownership is designed. |
| `WorldService` | Suggests service locator or dependency-injection semantics that are not established in the current World code. |
| `WorldKernel` | Existing roadmap language, but too close to internal simulation/kernel implementation and likely to repeat the current leakage. |

Recommended future facade name: `WorldFacade`.

Older roadmap references to `WorldKernel` should remain roadmap context only.
They should not drive the public facade name for this recovery phase.

## Transition Strategy

1. Keep the shell narrow and transitional. Do not treat it as a stable SDK
   guarantee.
2. Introduce real facade behavior incrementally while keeping the old cluster
   private.
3. Route any future public World access through `WorldFacade` instead of adding
   new public use of `WorldNode.h`.
4. Keep public signatures from exposing `SimCell`, scheduler, event stream,
   relevance graph, session, replication, dirty tracking, persistence/replay,
   or mutable world-state internals.
5. Keep `WorldNode`, `SimCell`, `DomainScheduler`, `EventStream`, and
   `RelevanceGraph` private unless a later compatibility audit finds a real
   need for a new public contract.

## Non-Goals

- no API guarantee yet;
- no runtime/server ownership change yet;
- no CMake/install change yet;
- no further header moves in this contract;
- no full World cleanup claim.

## Closure And Install Surface

World public-surface recovery is closed as a narrow slice. The public World
directory contains only `SagaEngine/World/WorldFacade.h`; all old World
internals listed above are private, and no forwarding headers were added.

`cmake/modules/SagaInstall.cmake` installs `Engine/Public/SagaEngine`
wholesale with `install(DIRECTORY "${SAGA_ROOT}/Engine/Public/SagaEngine" ...)`.
That rule was audited but not changed. With the current tree, the installed
World include surface would contain only `WorldFacade.h`.

## Next Implementation Slice

The next implementation slice should decide the first real behavior to route
through `WorldFacade`. That slice should keep the existing `WorldNode` cluster
private unless it is explicitly scoped as a compatibility phase, and should
continue to prove that the facade can express lifecycle, configuration, status,
minimal command submission, minimal read-only query, and diagnostics-safe
reporting without exposing the internal WorldNode/SimCell cluster.
