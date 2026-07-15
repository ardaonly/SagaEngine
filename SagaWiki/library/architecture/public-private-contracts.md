---
title: Public and private contracts
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Public and private contracts

`Public/` is an intentional contract surface, not a destination for every header that was visible before the cutover. A public header should be usable by its declared consumers without importing another module's private path or a concrete vendor implementation accidentally.

## Public surface

Good public candidates are interfaces, value types, stable configuration, opaque handles, events, and factories whose existence is part of the module contract. Their dependencies must be represented accurately in CMake and installed-consumer evidence.

## Private surface

Concrete adapters, vendor lifecycle owners, widgets, transport implementations, caches, bridges, and module-internal managers belong under `Private/` unless there is an explicit API decision to expose them. White-box tests may reach private helpers through deliberately configured test targets; this does not turn those helpers into installed API.

## Vendor boundary

Diligent device/context/swapchain and resource ownership stays private to RHI/Render implementation. The UI contract should not require RmlUi types, Persistence contracts should not require pqxx or Redis implementation types, and platform-neutral Input contracts should not require SDL details. Qt types are confined to the explicit EditorQt boundary.

The current repository may still contain transitional public surfaces that deserve review. Their location is evidence of current build visibility, not a promise that every such type is stable forever.

## No compatibility shadow tree

When an implementation becomes private, do not recreate the old path with forwarding aliases merely to preserve a mechanical layout. Preserve a public include only when an actual consumer contract has been demonstrated.

Detailed owner and public-surface rules are in [Ownership and dependency contracts](../reference/ownership-and-dependency-contracts.md). Graphics-specific native ownership is in [Rendering and graphics contracts](../reference/rendering-contracts.md).
