# SagaPreviewGate

SagaPreviewGate is the Phase 60-65 Technical Preview candidate evidence CLI.

It orchestrates existing focused evidence and writes deterministic local
reports. It does not run raw full CTest, implement gameplay/editor features,
apply patches, mutate C# source, create an installer, or claim beta/release
candidate status.

```sh
Tools/SagaPreviewGate/sagapreviewgate quickstart-check --out Build/TechnicalPreview/quickstart_report.json
Tools/SagaPreviewGate/sagapreviewgate accept --out-root Build/TechnicalPreview/Acceptance --bin-dir build/RelWithDebInfo-0.0.9/bin
Tools/SagaPreviewGate/sagapreviewgate build-matrix --out Build/TechnicalPreview/build_matrix_report.json --evidence Build/TechnicalPreview/Acceptance/mvp_acceptance_report.json
Tools/SagaPreviewGate/sagapreviewgate package --out-root Build/TechnicalPreview/Package
Tools/SagaPreviewGate/sagapreviewgate close --evidence-root Build/TechnicalPreview --out Build/TechnicalPreview/technical_preview_closure_report.json
```
