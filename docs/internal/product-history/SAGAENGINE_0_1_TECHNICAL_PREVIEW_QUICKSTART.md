# SagaEngine 0.1 Technical Preview Quickstart

Status: Phase 60 onboarding evidence for the local Technical Preview candidate.

This quickstart is for a developer working from the repository. It is not an
installer, updater, store package, commercial distribution, or hosted service.

## Scope

The supported first path is `samples/MultiplayerSandbox` with the
`local-server-headless` launch profile and the
`technical-preview-server-headless` package profile.

Expected local assumptions:

- repository checkout is present;
- `nix-shell` or an equivalent .NET/CMake toolchain is available;
- the focused native build directory is `build/RelWithDebInfo-0.0.9`;
- generated reports are written under `Build/TechnicalPreview`;
- C# source is canonical and is not rewritten by this path.

## Commands

Run the focused Technical Preview gate commands:

```sh
Tools/SagaPreviewGate/sagapreviewgate quickstart-check --out Build/TechnicalPreview/quickstart_report.json
Tools/SagaPreviewGate/sagapreviewgate accept --out-root Build/TechnicalPreview/Acceptance --bin-dir build/RelWithDebInfo-0.0.9/bin
Tools/SagaPreviewGate/sagapreviewgate build-matrix --out Build/TechnicalPreview/build_matrix_report.json --evidence Build/TechnicalPreview/Acceptance/mvp_acceptance_report.json
Tools/SagaDocGuard/sagadocguard scan --docs docs --evidence-root . --out Build/TechnicalPreview/docguard_report.json
Tools/SagaPreviewGate/sagapreviewgate package --out-root Build/TechnicalPreview/Package
Tools/SagaPreviewGate/sagapreviewgate close --evidence-root Build/TechnicalPreview --out Build/TechnicalPreview/technical_preview_closure_report.json
```

The acceptance command executes the existing focused tools for project
validation, launch proof, diagnostics summary, package proof, SagaScript
projection, view honesty, and documentation honesty.

## Non-Claims

- no beta product status;
- no release candidate;
- no production MMO server;
- no complete visual scripting;
- no arbitrary C# roundtrip;
- no enterprise readiness.

Additional boundaries:

- no production network readiness;
- no full editor MVP;
- no source rewrite or source-changing patch behavior;
- no full collaboration or hosted workspace service;
- no full security model;
- no installer, signing, updater, or store publishing path.
