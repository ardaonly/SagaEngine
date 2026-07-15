---
title: Rendering contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Rendering contracts

Render owns scene-facing rendering policy and RHI owns the lower-level device abstraction. Concrete Diligent integration is a private adapter shared through the intended runtime target, not an alternate public graphics API.

## Coordinate convention

SagaEngine uses a right-handed coordinate system: `+X` right, `+Y` up, and forward along `-Z`. Clip-space depth is zero to one and counter-clockwise winding identifies front faces. Camera, projection, culling, mesh import, and backend conversion must preserve this convention.

## Public extension boundary

Public rendering contracts should describe engine concepts: resources, passes, materials, views, lighting inputs, and extension points. They should not require consumers to own a Diligent device, context, swapchain, cache, or native resource lifetime.

An extension may contribute data or commands through a declared Render/RHI contract. It may not create a second native device lifecycle or bypass engine resource ownership. Backend selection is configuration interpreted by the owning RHI implementation, not a guarantee that every named graphics API has equal support.

The public graphics vocabulary expresses backend preference without vendor names. Capability fallback is conservative and deterministic: a capability that was not requested is `NotRequested`, a requested and supported capability is `Available`, and a requested but unsupported capability is `DisabledUnsupported`. Focused mapping tests protect this behavior; the matrix describes reported capability, not identical backend feature depth.

## World and interest

World visibility and replication interest are related inputs but not interchangeable truths. Rendering may consume a render-interest view derived from world state. It must not mutate authority or make persistence decisions merely because an object is visible.

## Materials and lighting

Material graphs and lighting strategy are authoring and rendering contracts only to the extent supported by current types and evidence. Generated shader/material output remains derived from project source. Historical strategy documents can guide vocabulary, but they do not turn planned shading models or editor workflows into implemented capability.

## Evidence

Architecture tests protect vendor/private boundaries and installed-consumer tests protect public include usability. GPU tests prove particular backend paths under their environment; they are distinct from portable contract evidence.

See [Rendering and graphics contracts](../reference/rendering-contracts.md) for the full public/private API, coordinate, lifecycle, frame, interest, residency, materials, lighting, graph, capture, and evidence contract.
