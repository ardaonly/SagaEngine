# Architecture Boundary Risk Inventory

> Status: Current report-only boundary risk inventory
> Scope: Product, Runtime/App, Sandbox, and test graph boundary risks

This document does not change build behavior or claim that these boundaries are
already clean. It records ownership risks that must stay visible before code
refactors continue.

Use this document with [CMake Boundary Inventory](CMAKE_BOUNDARY_INVENTORY.md).
The CMake inventory records current build-graph evidence; this document records
the architecture meaning of the highest-risk ownership drifts.

## Risk Areas

1. Audit `SagaProductLib` as a product dependency hub.
2. Preserve the independent `SagaRuntime` application boundary.
3. Keep `SagaSandboxLib` out of architecture-clean proof claims.
4. Separate broad test health from boundary and conformance evidence.

## SagaProductLib

Current evidence: `CMAKE_BOUNDARY_INVENTORY.md` classifies `SagaProductLib` as
CBI-008 and HBI-002.

`SagaProductLib` currently exports `Apps/Saga` and publicly republishes major
modules including Engine, Shared, Collaboration, Runtime, Server, and Qt. That
makes it a product dependency hub, not a thin orchestration layer.

Accepted current role:

- Product Shell may route workflows and preserve command/report/diagnostic
  evidence.
- Product Shell may coordinate public module boundaries.
- Product Shell must not be described as owning Runtime, Server, Editor, Qt UI,
  compiler, package, or distribution implementation.

Report-only follow-up:

- Run a report-only Product ABI/dependency audit before changing CMake
  visibility.
- Identify which public `Apps/Saga` headers actually require Runtime, Server,
  Collaboration, and Qt-shaped contracts.
- Keep any `PUBLIC` to `PRIVATE` conversion deferred until public-header
  evidence shows it is safe.

Non-goal:

- Do not refactor `SagaProductLib` in the same slice as this report.

## SagaRuntime Application Boundary

Current evidence: `SagaRuntime` owns its app-local sources and architecture
tests reject sources or includes from other application roots.

The legacy client executable and its app-private host were retired after the
Runtime host was extracted. Runtime no longer compiles or includes client app
sources, and no compatibility target remains.

Required contract:

```txt
Runtime -> Runtime-owned libraries and app-local composition
Runtime -X-> other application implementation roots
```

Runtime may expose reusable startup, package, asset, lifecycle, and service
contracts. It must not depend on sources owned by another executable as its
execution model.

Enforced boundary:

- `SagaRuntime` sources and includes remain Runtime-owned.
- Product Shell and Editor targets do not compile or include retired client
  app paths.
- No legacy client target, output identity, install rule, or package entry may
  be restored implicitly.

Non-goal:

- This boundary does not claim a generic connected-client loop, client preview,
  production networking, or complete runtime gameplay.

## SagaSandboxLib

Current evidence: `CMAKE_BOUNDARY_INVENTORY.md` classifies `SagaSandboxLib` as
CBI-010, and [Apps/Sandbox Role](APPS_SANDBOX_ROLE.md) defines it as an
internal diagnostic sandbox.

`SagaSandboxLib` intentionally composes Engine, Runtime, Server, Backend, and
diagnostic code. That can be acceptable for a playground or harness, but it
must not be used as proof that the production architecture is clean.

Accepted current role:

- Sandbox may be a manual diagnostic playground.
- Sandbox may wire multiple subsystems together for probe scenarios.
- Sandbox evidence can support local development diagnostics only when the
  claim states that limited scope.

Forbidden claim:

- `SagaSandboxLib` is not architecture-clean proof.

Report-only follow-up:

- Keep Sandbox cleanup separate from Product, Runtime, and Server ownership
  hardening.
- Move deterministic sandbox probes into tests only when replacement coverage
  exists.

Non-goal:

- Do not narrow `SagaSandboxLib` dependencies in this report-only slice.

## Test Graph

Current evidence: `CMAKE_BOUNDARY_INVENTORY.md` classifies broad test include
and link access as TBI-001 through TBI-006.

The current broad test graph can be useful for regression coverage, but it can
also hide boundary violations. A test target that can include nearly every
module cannot prove that production targets obey the same boundaries.

Accepted current role:

- Wide tests may remain as integration-like regression coverage.
- Architecture and boundary claims require focused boundary or conformance
  checks.
- Broad `SagaUnitTests` success must not be used as proof of clean Product,
  Runtime, Editor, Server, or Backend boundaries.

Report-only follow-up:

- Add or narrow boundary/conformance tests only after each boundary contract is
  written.
- Keep existing broad tests available for regression coverage.
- Prefer new narrow targets for migrated boundaries instead of splitting the
  whole test graph at once.

Non-goal:

- Do not split `SagaUnitTests` or rewrite the test graph in this report-only
  slice.

## Verification

For this report-only slice, acceptable verification is:

```bash
python3 Tools/scripts/check_public_private_boundary.py --repo-root .
python3 Tools/scripts/check_qt_boundary.py .
scripts/test-taxonomy --check
git diff --check
```

These checks do not prove that the boundaries above are clean. They prove only
that this documentation update did not weaken the existing boundary guards or
test taxonomy metadata.
