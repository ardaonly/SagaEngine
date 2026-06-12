# Ownership And Boundaries

> Last updated: 2026-05-26

SagaEngine has boundary and ownership foundations, but several ownership loops
remain incomplete. Treat this as current architecture context, not product
readiness.

## Current Boundary References

- [CMAKE_BOUNDARY_INVENTORY.md](CMAKE_BOUNDARY_INVENTORY.md) remains the active
  architecture boundary inventory.
- Runtime startup/lifecycle ownership has a foundation, but complete
  Runtime/App ownership is not proven.
- Server authoritative movement has a foundation, but full product server
  replication/snapshot/reconciliation is not proven.
- Asset/package/runtime readiness has a foundation, but full AssetPipeline
  source import/cook and ClientHost asset consumption are not proven.
- Editor public Qt exposure is reduced and guarded, with explicit remaining
  public Qt debt.

## Current Evidence Boundaries

- [Runtime](RUNTIME.md)
- [Server authority](SERVER.md)
- [Assets and packages](ASSETS_AND_PACKAGES.md)
- [Editor](EDITOR.md)
