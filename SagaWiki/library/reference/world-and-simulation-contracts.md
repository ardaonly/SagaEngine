---
title: World and simulation contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# World and simulation contracts

The World module owns world organization and a narrow facade; Simulation and ECS own behavioral state and deterministic updates. This reference preserves the durable public-facade and source-truth rules without presenting planned large-world behavior as shipped persistent-world capability.

## Public facade

`SagaEngine/World/WorldFacade.h` is the current public entry boundary. It exposes small configuration, lifecycle/tick, status/stat, and command-shaped values while private partitioning and streaming details remain behind the owner. The facade is transitional and deliberately narrow.

A public facade should let a consumer ask the world owner to initialize, advance, inspect bounded status, submit supported commands, and shut down. It should not return mutable references to private cell maps, partition trees, streaming queues, persistence sessions, replication internals, or native resources.

Adding a public method requires a stable semantic question. “Give me the internal manager so I can call it” is not a semantic contract. Prefer copied configuration, command, result, event, status, and snapshot values with explicit ownership.

## Truth layers

Several representations coexist:

| Representation | Role |
| --- | --- |
| Scene/entity source files | Authoring truth for declared project content |
| Validated scene/entity model | Parsed source with schema and reference checks |
| ECS/simulation state | Live behavioral truth for an active runtime |
| World partition/cell state | Spatial organization and streaming bookkeeping |
| Replication snapshot/client state | Network-derived representation bounded by authority rules |
| Render world/frame snapshot | Derived presentation input |
| Persistence records/events | Durable storage representation where implemented |

No derived view silently writes back to another authority. The renderer does not delete an ECS entity because it is culled. A client interpolation buffer does not become server truth. A persistence record does not overwrite live state without an explicit load/reconciliation transaction.

## Scene activation

Runtime scene activation begins with validated project and scene inputs. The loader verifies schema, entity identifiers, component shapes, asset references, and project-relative paths. It resolves supported assets through manifests/registries rather than directory guesses. Only then does an owner construct or update runtime entities.

Activation should be transactional for required content. If entity `N` fails schema/reference validation, the runtime does not present a partially loaded scene as complete unless the mode explicitly supports partial diagnostic inspection. Diagnostics identify the scene, entity, field/reference, and failure class.

Generated scene artifacts are derived. Their source fingerprint and schema/tool contract are checked before use. The runtime never silently edits scene source to fit a generated artifact.

## ECS and simulation

ECS owns entity/component storage and access rules. Simulation owns systems and time-step policy that mutate behavioral state. World organizes which parts are active/relevant and conveys supported events or commands. These owners may be composed tightly, but their contracts remain distinct.

Fixed-step simulation should advance with a declared tick duration and bounded catch-up policy. Variable presentation frames can interpolate between authoritative/simulated states. Large elapsed-time spikes do not justify an unbounded loop that freezes the process; the owner reports dropped/clamped work according to policy.

Systems execute in a deterministic declared order where behavior depends on order. Cross-thread mutation occurs through queues/snapshots or an explicit scheduler contract, not through unsynchronized component references.

## World cells and partitioning

Private world implementation may map positions/entities to cells, maintain neighbor relationships, evaluate active/loaded sets, and publish transitions. Cell identity is stable within its declared world/project domain. Coordinate-to-cell conversion follows the engine coordinate convention and deterministic boundary rules.

Partitioning is an organization tool, not authority. An entity crossing a cell boundary remains the same entity. Moving between server-authority zones may require a separate transfer protocol; moving between render streaming cells may only affect content residency.

Cell activation can request resources and create runtime entities. Deactivation stops new work, releases borrows after consumers quiesce, and preserves any required state through the appropriate authority/persistence owner. A cell is not called persisted merely because its bytes can be reloaded from a package.

## Relevance boundaries

World relevance can inform networking, replication, resource loading, and rendering, but each owner applies its own policy. A world query can provide candidate entities/cells for a location. Replication evaluates what the server sends. Render evaluates view visibility. Resources evaluates byte/device residency.

The data crossing those seams should be copied identifiers and bounded snapshots rather than private container access. This prevents one subsystem from changing the world while another iterates its internal structure.

## Commands and events

Commands request mutation; events report a committed fact. Both identify their world/project context, target identity, operation kind, and payload schema. A command result distinguishes accepted, rejected, unsupported, stale, or failed. Unsupported commands remain explicit rather than treated as successful no-ops.

World events are ordered according to the owner’s tick/transaction rules. Observers must not assume delivery means durability or network acknowledgement unless those guarantees are part of the event contract.

## Loading, unloading, and cancellation

World streaming work carries a generation or activation token. If a cell is no longer required while its resources load, cancellation prevents late completion from activating it. If cancellation cannot stop underlying I/O, the late result is discarded against the generation and released safely.

Unload waits for or detaches owned work according to the lifetime contract. It does not destroy resources still borrowed by a render frame or active system. Resource handles and snapshot lifetimes provide the handoff.

## Status and diagnostics

World status should answer initialized/running/stopping/failure state plus bounded counts such as active cells, loaded cells, pending transitions, entity totals, rejected commands, and last diagnostic identifier. Detailed dumps are derived diagnostics and must not expose mutable implementation.

Useful diagnostics distinguish source validation failure, missing asset, cell activation failure, cancelled transition, stale completion, simulation overrun, invalid command, and authority rejection. Labels remain bounded; raw entity names or arbitrary paths are included only when safe and useful.

## Evidence

Facade compile/installed-consumer tests prove public usability. Unit tests prove coordinate/cell boundaries, deterministic transition order, command results, cancellation/generation behavior, and status accounting. Integration tests can load a small validated scene and exercise activation/deactivation. Replication and render tests prove their own derived views.

This evidence does not establish infinite worlds, production world persistence, seamless distributed zone transfer, arbitrary scene compatibility, or a complete editor world-authoring product.

## Non-claims

The current World owner is not evidence of shipped persistent community worlds. No SagaServer program or dedicated-server executable is currently provided. ServerAuthority types do not by themselves prove a deployed service. World partition types do not prove production-scale streaming or persistence.

## Change checklist

- Keep the public facade semantic and narrow.
- Keep partition/streaming containers private.
- Identify scene source, live simulation, network, render, and persistence truth separately.
- Make activation, cancellation, and late completion deterministic.
- Preserve entity identity across spatial representation changes.
- Use copied commands/events/status rather than exposing managers.
- Match claims to focused evidence and retain explicit non-claims.
