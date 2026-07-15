---
title: Capability boundaries and non-claims
status: policy
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Capability boundaries and non-claims

SagaEngine contains substantial foundations, but foundations must not be described as completed products. The following boundaries stay explicit until implementation and evidence change them.

## Product and SDK

- no finished public SDK or compatibility promise;
- no complete cross-platform installer, launcher, or release channel;
- no stable third-party plugin API or marketplace contract;
- no claim that every public header is permanently stable.

## Editor and authoring

- no finished end-to-end editor product;
- no complete asset/scene workflow across all intended formats;
- no shipped production collaborative editing service;
- no hosted collaboration security, tenancy, or operations claim;
- Visual Blocks are not a second runtime or graph VM.

## Networking and persistence

- no production networking stack or internet-scale service claim;
- no shipped persistent community-world service;
- no complete database migration, backup, or operational topology;
- the SagaServer program name does not by itself prove a dedicated-server product.

## Rendering and platforms

- no equal-support guarantee for every graphics backend or platform;
- no promise that every historical material, lighting, streaming, or render strategy is implemented;
- GPU evidence is limited to the tested hardware, driver, and configuration.

## Content and samples

- samples demonstrate named workflows, not production content volume or polish;
- generated files and evidence reports are not independent source authorities;
- historical plans, inventories, audits, and migration closures are not current capability descriptions.

These are accuracy boundaries, not a roadmap. Future work becomes current only after the owning implementation, contract, and evidence are present.

For detailed capability/evidence levels and current product wording, see [Product and capability reference](../reference/product-and-capability-boundaries.md).
