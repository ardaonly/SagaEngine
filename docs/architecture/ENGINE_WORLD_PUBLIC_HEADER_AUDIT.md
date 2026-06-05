# Engine World Public Header Audit

> Last updated: 2026-06-05

This file started as the report-only migration audit for
`Engine/Public/SagaEngine/World`. The first implementation slice moved the
low-risk Partition/Streaming cluster, and a later WorldFacade-backed slice moved
the old WorldNode/SimCell cluster private.

The World public surface has been narrowed to a transitional facade shell. This
document does not claim a stable SDK/API or full Engine public API cleanup.

## Method

The audit used direct include/reference inspection only. Direct includes are the
main signal because broad symbol searches for names such as `Cell`, `Zone`, or
`Boundary` produce unrelated matches in server, editor, rendering, and tests.

Read-only commands used during inspection:

```sh
find Engine/Public/SagaEngine/World -type f
rg -n '#include .*SagaEngine/World|#include <SagaEngine/World|#include "SagaEngine/World' . -g '!build/**'
rg -n 'WorldNode|SimCell|DomainScheduler|RelevanceGraph|Streaming|Partition' Engine Tests Apps Tools Server Runtime Editor
```

Original audit summary:

- 10 headers exist under `Engine/Public/SagaEngine/World`.
- No World header is directly included outside `Engine`.
- No World header is directly included by `Tests`, `Tools`, `Runtime`,
  `Server`, `Editor`, or `Apps`.
- Direct include use is limited to World public headers including each other
  and matching implementation files under `Engine/Private/SagaEngine/World`.

Tests using a public header would not automatically make that header true SDK
API. In this audit, there are no direct test includes for the World headers.

## After-State

The first migration slice moved these headers to matching paths under
`Engine/Private/SagaEngine/World`:

- `SagaEngine/World/Partition/Cell.h`
- `SagaEngine/World/Partition/Boundary.h`
- `SagaEngine/World/Partition/Zone.h`
- `SagaEngine/World/Streaming/LODManager.h`
- `SagaEngine/World/Streaming/ResourceStream.h`

The WorldFacade-backed migration slice then moved these headers to matching
paths under `Engine/Private/SagaEngine/World`:

- `SagaEngine/World/DomainScheduler.h`
- `SagaEngine/World/EventStream.h`
- `SagaEngine/World/RelevanceGraph.h`
- `SagaEngine/World/SimCell.h`
- `SagaEngine/World/WorldNode.h`

No forwarding headers were added. No CMake install rules were changed.
`SagaEngine/World/WorldFacade.h` is now the only public World header.

## File-Level Decision Matrix

| Header | Outside Engine direct includes | Inside Engine direct includes | Tests/Tools/Runtime/Server/Editor usage | Classification | Recommended action | Risk | Reason |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `SagaEngine/World/Partition/Cell.h` | None | Public before move: `Boundary.h`, `Zone.h`, `ResourceStream.h`; Private: `Cell.cpp` | None by direct include | Internal implementation detail | Moved Private | Low | Cell descriptors are world streaming internals and not current SDK API. Moved with Partition/Streaming cluster to avoid public headers including private headers. |
| `SagaEngine/World/Partition/Boundary.h` | None | Public before move: `ResourceStream.h`; Private: `Boundary.cpp` | None by direct include | Internal implementation detail | Moved Private | Low | Boundary rings are loader residency policy, not public engine API. Moved with `Cell.h` and `ResourceStream.h`. |
| `SagaEngine/World/Partition/Zone.h` | None | Public before move: `ResourceStream.h`; Private: `Zone.cpp` | None by direct include | Internal implementation detail | Moved Private | Low | Zone grid/cell lookup is only consumed by World streaming internals in this tree. |
| `SagaEngine/World/Streaming/LODManager.h` | None | Public before move: `ResourceStream.h`; Private: `LODManager.cpp` | None by direct include | Internal implementation detail | Moved Private | Low | LOD selection is a world streaming/render implementation helper, not a stable public render extension API. |
| `SagaEngine/World/Streaming/ResourceStream.h` | None | Private: `ResourceStream.cpp` | None by direct include | Internal implementation detail | Moved Private | Low | It is an orchestrator over ResourceManager, Zone, Boundary, and LODManager; no external module includes it directly. |
| `SagaEngine/World/DomainScheduler.h` | None | Public before move: `WorldNode.h`; Private: `DomainScheduler.cpp` | None by direct include | Internal implementation detail | Moved Private | Medium | Scheduler was only exposed because `WorldNode.h` was public. Moved after the transitional facade shell existed. |
| `SagaEngine/World/EventStream.h` | None | Public before move: `WorldNode.h`; Private: `EventStream.cpp` | None by direct include | Internal implementation detail | Moved Private | Medium | Event persistence/replay is owned by WorldNode internals; future public access should go through the facade. |
| `SagaEngine/World/RelevanceGraph.h` | None | Public before move: `WorldNode.h`; Private: `RelevanceGraph.cpp` | None by direct include | Internal implementation detail | Moved Private | Medium | Interest graph is replication/world logic internals and was public only through WorldNode composition. |
| `SagaEngine/World/SimCell.h` | None | Public before move: `DomainScheduler.h`, `EventStream.h`, `RelevanceGraph.h`, `WorldNode.h`; Private: `SimCell.cpp`, `SimCell_SpatialLogic.cpp` | None by direct include | Internal implementation detail | Moved Private | Medium | Central dependency for most World headers. Moved with the full cluster after the facade shell existed. |
| `SagaEngine/World/WorldNode.h` | None | Private: `WorldNode.cpp` | None by direct include | Internal implementation detail behind facade | Moved Private | High | It presents a server-like world kernel with replication/session details, so it now sits behind the transitional facade shell. |

## Completed First Implementation Slice

The first implementation phase moved only the self-contained
Partition/Streaming cluster:

- `SagaEngine/World/Partition/Cell.h`
- `SagaEngine/World/Partition/Boundary.h`
- `SagaEngine/World/Partition/Zone.h`
- `SagaEngine/World/Streaming/LODManager.h`
- `SagaEngine/World/Streaming/ResourceStream.h`

These headers had no direct includes outside `Engine`, no direct
Tests/Tools/Runtime/Server/Editor/App includes, and no dependency on
`WorldNode.h`. They were moved as one cluster so no public World header is left
including a private World header.

The first implementation slice intentionally did not move these headers:

- `SagaEngine/World/SimCell.h`
- `SagaEngine/World/DomainScheduler.h`
- `SagaEngine/World/EventStream.h`
- `SagaEngine/World/RelevanceGraph.h`
- `SagaEngine/World/WorldNode.h`

Those headers were more coupled and were handled later as a
WorldNode/SimCell facade pass. See the focused
[Engine WorldNode / SimCell facade audit](ENGINE_WORLD_NODE_FACADE_AUDIT.md)
for the direct include matrix, leakage analysis, and recommended Option B
sequence.

## Remaining Work

`SagaEngine/World` is no longer exposing the old WorldNode/SimCell cluster.
The intended public World surface is now:

- `SagaEngine/World/WorldFacade.h`

The next World cleanup should add real facade behavior without exposing the
private cluster. The intended boundary is defined in
[Engine World public facade contract](ENGINE_WORLD_PUBLIC_FACADE_CONTRACT.md).

## Install Surface Audit

`cmake/modules/SagaInstall.cmake` still installs
`Engine/Public/SagaEngine` wholesale through `install(DIRECTORY
"${SAGA_ROOT}/Engine/Public/SagaEngine" ...)`. No install rule was changed for
this World recovery slice.

Because `Engine/Public/SagaEngine/World` now contains only `WorldFacade.h`, the
current installed World include surface would also include only
`SagaEngine/World/WorldFacade.h`. That header is transitional and is not a
stable SDK/API guarantee.

## Compatibility Notes

No old WorldNode/SimCell cluster header is currently directly included outside
`Engine`. If a later audit finds an outside include, do not add a forwarding
header without a compatibility strategy.

Tests should be treated as validation consumers, not SDK consumers. A future
test-only direct include should be recorded separately and should not by itself
force a header to remain public.

## Status

This audit records the completed Partition/Streaming cluster move and the later
WorldNode/SimCell cluster private migration. It does not claim a stable SDK/API
or full Engine public API cleanup.

World public-surface recovery is closed as a narrow slice: no old World
internal header remains public, no forwarding headers were added, and remaining
work is broader Engine public API cleanup plus future `WorldFacade` behavior.
