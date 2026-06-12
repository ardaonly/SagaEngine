# Render Interest / World Streaming Contract

> Last updated: 2026-06-12

This document defines the CPU-side boundary between MMO world/network
interest, client streaming, render visibility, and render resource residency.
It is a contract and diagnostics foundation, not a terrain renderer or native
streaming implementation.

## Contract

The render pipeline treats these states as related but distinct:

- Network relevance: the server or replication layer says an entity matters to
  the client simulation.
- Client streaming relevance: the client predicts that a zone, chunk, mesh, or
  texture may be needed soon.
- Render visibility: a view may draw the entity after camera, layer, distance,
  and culling policy.
- Render resource residency: render-facing CPU decisions say which mip or LOD
  should be requested or which fallback is acceptable.

The intended lifecycle is:

```text
entity discovered
-> preload hinted
-> enters render visibility
-> resources resident or fallback diagnostic
-> leaves render visibility
-> release hinted
```

`RenderInterestWorldStreaming.h` is the public DTO/API surface for this
contract. It may carry preload hints and deterministic diagnostics. It must not
own asset bytes, backend resources, native descriptors, GPU uploads, terrain
chunks, or network authority.

## Non-Claims

This contract does not implement a terrain renderer.
It does not prove unloaded-zone fallback render smoke.
It does not make network relevance the same thing as render relevance.
It does not replace `Resources::StreamingManager`, `ResourceManager`,
`TextureStreamer`, `MeshStreamer`, or `StreamingBudget`.
It does not complete GPU streaming behavior.
It does not implement virtual texture residency.
