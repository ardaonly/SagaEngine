# SagaEngine Technical Preview Product Definition

> Milestone 13 evidence for Target 1 / Technical Preview / Block B.

Block B opens product work from the accepted diagnostics foundation. The
Technical Preview product spine is a project-truth layer, not a launch,
networking, editor, or packaging completion claim.

## Claim Level

The Technical Preview may claim:

- a canonical project manifest path exists for product work;
- project manifests can be validated by deterministic local tooling;
- project-relative paths can be resolved into a stable report;
- project health can be checked without repairing or rewriting files;
- `samples/MultiplayerSandbox` is the first product proof fixture;
- Block A diagnostics evidence remains the foundation for future product work.

The Technical Preview must not claim:

- beta status or release-candidate status;
- production MMO readiness;
- production networking readiness;
- complete node authoring;
- arbitrary C# roundtrip into blocks;
- enterprise readiness;
- complete editor MVP status;
- complete asset pipeline status;
- package or publish readiness;
- raw complete CTest health unless that exact run is performed and recorded.

## First Product Proof

The first product proof path is:

```text
samples/MultiplayerSandbox
```

For Milestone 19 this path is a fixture only. It is valid project truth for
SagaProjectKit validation, resolution, and doctor checks. It is not playable
game content and does not imply runtime/server launch behavior.

## Artifact Ownership

| Artifact | Owner | Milestone 13 meaning |
| --- | --- | --- |
| `.sagaproj` | Project truth | Canonical Technical Preview project manifest. |
| `launch_profiles.json` | Project truth | Declarative launch intent only. |
| `package_profiles.json` | Project truth | Declarative package intent only. |
| `Scripts/` | Project source | Placeholder or future script sources. |
| `Assets/` | Project source | Placeholder or future asset sources. |
| `Diagnostics/` | Project output convention | Reserved diagnostics output folder. |
| `Build/Reports/` | Generated output convention | Reserved generated report folder. |
| `saga.project.json` | Existing product shell | Existing Apps/Saga behavior, not replaced here. |

## Path Decisions

- Canonical sample root: `samples/MultiplayerSandbox`.
- Canonical manifest name: `MultiplayerSandbox.sagaproj`.
- Tool root: `Tools/SagaProjectKit`.
- Generated report names:
  - `project_validation_report.json`
  - `project_resolution.json`
  - `project_doctor_report.json`
- Generated reports should be written outside source truth unless a future milestone
  explicitly accepts a fixture.

## Block A Reference

The opening evidence is recorded in
`docs/architecture/DIAGNOSTICS_MIGRATION_AUDIT.md`.
Block A establishes diagnostics foundation only; it does not establish
production reliability, production networking, launch readiness, or a product
ship gate.
