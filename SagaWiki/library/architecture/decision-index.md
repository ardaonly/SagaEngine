---
title: Architecture decision index
status: policy
owner: SagaWiki
reviewed_against: 0.0.11-hibernation
---

# Architecture decision index

This index makes stable choices discoverable without introducing a separate ADR
process. Implementation and tests remain stronger evidence than this summary.

| Decision | Current rule | Authority/evidence |
| --- | --- | --- |
| Editor frontend | Qt6 Widgets is the permanent Editor frontend. | EditorQt ownership and Qt architecture tests |
| Process boundary | Editor and Runtime remain separate processes. | Product workflow and Runtime lifecycle contracts |
| Gameplay source | C# is canonical high-level gameplay authority. | [C# and Visual Blocks contracts](../reference/csharp-and-visual-blocks-contracts.md) |
| Controlled Blocks | Visual Blocks project and edit supported C#; there is no Blocks VM. | Script projection and read-only/controlled workflow tests |
| Runtime input | Runtime consumes verified artifacts and does not author projects or packages. | [Source and artifact contracts](../reference/source-artifact-and-project-contracts.md) |
| Package graph | `.sagaproj` declares intent; `SagaPackage.lock` is resolved package authority, including ownership and visibility metadata. | SagaProjectKit and SagaPackager contracts |
| Managed tools | Saga uses a pinned private .NET runtime in product distributions; a global runtime is not a product fallback. | Managed runtime manifest and distribution checks |
| Standalone builds | One immutable input snapshot flows through private tools, owned staging, verification, and recoverable publication. | [Build, package, and publish](../toolchain/build-package-publish.md) |
| Reproducibility | Semantic content identity is content-addressed and distinct from publication-local envelope bytes. | Standalone manifest and reproducibility contracts |
| Workspace state | Work mode and layout are presentation state, not project gameplay truth. | Workspace service and Qt workflow tests |
| Conversion history | Controlled conversion history is separate from global Editor Undo/Redo. | Script projection/conversion contracts |
| Test authority | CMake/CTest owns native tests and suites; Python and future workflow YAML only orchestrate. | `cmake/testing/SagaTestSuites.cmake` and testing roadmap |
