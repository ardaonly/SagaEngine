# Phase 11E Post-Recovery Roadmap

> Last updated: 2026-05-26
> Status: Phase 11E post-recovery roadmap recommended
> Phase 11: Scoring Re-Audit

Phase 11E recommends what to do after the recovery roadmap. It does not start
post-recovery implementation.

## Priority Order

1. Track A - Product Vertical Proof
2. Track B - Runtime/ClientHost Asset Consumption
3. Track C - AssetPipeline Source Import/Cook
4. Track D - Server/Replication Product Loop
5. Track F - Test/CI Hardening
6. Track E - Editor Productization
7. Track G - Release/Packaging Hardening

## Track A - Product Vertical Proof

Goal: prove a minimal packaged playable vertical, likely MultiplayerSandbox.

Why it matters: this removes the largest blocker to product beta maturity.

Entry criteria: Phase 10 report-first publish readiness, Phase 9 normal gate,
and focused Runtime/Server/Asset foundations.

Exit criteria: package-driven startup launches a minimal Runtime + Server +
ClientHost + UI loop with documented publish report integration.

First slices:

1. MultiplayerSandbox package/playable proof audit
2. minimal product package fixture and startup path
3. focused playable loop smoke test

Risks: broad product scope, server/client integration churn, asset consumption
gaps.

Verification: focused product smoke tests, package/publish/runtime focused
tests, and normal gate when safe.

Priority: immediate.

## Track B - Runtime/ClientHost Asset Consumption

Goal: connect RuntimeAssetCatalog and package assets to ClientHost through a
bounded access facade and service lifecycle.

First slices:

1. Runtime/ClientHost asset consumption audit
2. ClientHost asset access facade
3. RuntimeServiceRegistry asset service design/proof

Priority: immediate/near-term.

## Track C - AssetPipeline Source Import/Cook

Goal: turn manifest writing into source discovery, import, cook, artifact
production, content hashing/versioning, and generated package integration.

First slices:

1. source discovery/import audit
2. first cooked texture artifact pipeline
3. generated package integration proof

Priority: near-term.

## Track D - Server/Replication Product Loop

Goal: connect authoritative movement, replication, snapshots, and client
reconciliation into product multiplayer behavior.

First slices:

1. ReplicationManager dirty integration audit
2. accepted-state snapshot proof
3. client reconciliation smoke proof

Priority: near-term.

## Track E - Editor Productization

Goal: move editor from guarded/scaffolded foundations toward product workflows.

First slices:

1. GraphCanvas boundary audit
2. ProjectBrowser real file-tree/workspace behavior
3. dashboard diagnostic rows and visual scripting workflow proof

Priority: medium-term.

## Track F - Test/CI Hardening

Goal: turn focused local evidence into stable CI policy without overclaiming.
This is the CI hardening track, not release readiness.

First slices:

1. raw full CTest policy resolution audit
2. heavy gate opt-in stabilization
3. CI candidate workflow for deterministic focused gates

Priority: near-term.

## Track G - Release/Packaging Hardening

Goal: prepare release packaging only after product proof exists.

First slices:

1. release packaging audit
2. dependency/license audit
3. install/update and crash/logging diagnostics proof

Priority: later.

## What Not To Start Yet

Do not start release candidate work, broad editor rewrites, heavy gate default
CI, raw full CTest reruns, or full AssetPipeline redesign before a product
vertical proof audit and runtime/client asset consumption plan.

## First Recommended Slice

First recommended post-recovery slice: Product Vertical Proof 1A -
MultiplayerSandbox package/playable proof audit.

This should be docs/report-first and should not implement product behavior
until entry points, fixtures, package requirements, runtime/server/client
composition, and verification gates are mapped.

## Recommended Phase 11F

Proceed to Phase 11 closure. The post-recovery roadmap is clear enough to close
the recovery roadmap as Foundation Established.
