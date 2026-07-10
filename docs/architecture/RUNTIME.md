# Runtime

> Last updated: 2026-05-26

Runtime startup, lifecycle, package startup validation, and asset bootstrap are
partly implemented. This does not yet make a complete runtime product path.

## Runtime Asset Boundary

Runtime owns runtime loading, residency, runtime-ready manifest consumption,
and runtime diagnostics for load failures. It may consume package and asset
manifests produced by other layers.

Runtime does not own editor import/cook workflows, package staging, publish
readiness, repository-analysis tooling, Forge orchestration, source asset
normalization, or asset authoring UI. Those responsibilities remain with the
asset pipeline, tools, publish contracts, and editor surfaces.

## Current State

- Runtime startup/session/lifecycle code exists.
- Runtime package validation and local startup asset bootstrap code exists.
- Runtime read-only asset access exists.
- ClientHost/runtime asset consumption is incomplete.
- RuntimeServiceRegistry asset service is incomplete.
- A full playable runtime flow is missing.

## Evidence Boundary

Current evidence supports runtime startup, lifecycle, package validation, and
local asset bootstrap foundations only. It does not prove a complete client
loop, complete gameplay runtime, production networking, or package-driven
playability.

For implementation history and rationale, see the appendix-style
[Asset Streaming System implementation note](../internal/architecture-appendices/AssetStreamingImplementation.md).
That note is not the current runtime/asset ownership source of truth.
