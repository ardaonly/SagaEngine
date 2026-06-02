# Saga Project Model Inventory

> Phase 14 evidence for Target 1 / Technical Preview / Block B.

This inventory records existing project, package, runtime, and tool metadata
before introducing `.sagaproj` schema v0.

## Existing Project Systems

| System | Current path | Current ownership |
| --- | --- | --- |
| Apps/Saga project shell | `Apps/Saga/SagaProjectSystem.*` | Creates and opens `saga.project.json`; manages recent projects and local session labels. |
| Product shell project manifest | `<Project>/saga.project.json` | Existing shell metadata with `projectId`, `displayName`, and `sdeRoot`. |
| SDE workspace | `<Project>/.sde` | Project-authored workspace/customization source used by the product shell. |
| SagaScript tool | `Tools/SagaScript` | Script metadata validation and manifest emission. |
| Package manifests | `Tests/Fixtures/Packages/RuntimeSmoke` and package APIs | Runtime package metadata for package/bootstrap tests. |
| Publish readiness | `Apps/Saga/SagaPublishReadiness.*` | Report-first package and asset readiness checks. |

## Current `saga.project.json` Ownership

`saga.project.json` remains owned by `Apps/Saga/SagaProjectSystem`. It is the
existing product-shell create/open manifest and is not removed, renamed, or
rewritten by Block B.

Current responsibilities:

- store a stable product-shell `projectId`;
- store a display name;
- locate the project `.sde` root;
- support recent-project and local product-shell workflows.

## New `.sagaproj` Ownership

`.sagaproj` becomes the Technical Preview project truth manifest for
SagaProjectKit and future product phases.

Phase 15 ownership:

- describe project identity;
- describe engine compatibility intent;
- describe project-relative scene, asset, script, launch profile, package
  profile, diagnostics, and report paths;
- validate and resolve project truth without mutating files.

## Required MVP Fields

- `schemaVersion`
- `projectId`
- `displayName`
- `engineCompatibility`
- `paths.diagnostics`
- `paths.generatedReports`
- `scenes`
- `assets`
- `scriptFolders`
- `launchProfiles`
- `packageProfiles`

## Overlap And Debt

- `saga.project.json` and `.sagaproj` both describe project identity. Phase 15
  does not merge them.
- Future product-shell work must decide whether Apps/Saga opens `.sagaproj`,
  bridges both files, or generates one from a higher-level project workflow.
- Existing package manifests are runtime/package contracts, not project
  profile contracts.
- Launch profiles in Phase 19 are declarative only and are not consumed by a
  launcher.
- Package profiles in Phase 19 are declarative only and are not consumed by
  package staging or publish tooling.

## Migration Risks

- Existing editor/product shell code may continue to require
  `saga.project.json` until a future integration phase.
- Tooling that assumes `<Project>/Build` or `<Project>/Packages` may need a
  future resolver bridge.
- Future launch/package phases must avoid treating Phase 19 placeholder profile
  fields as execution contracts.
