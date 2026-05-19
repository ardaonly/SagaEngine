# Iteration Notes

> Status: Active development note
> Current iteration: `0.0.8-dev.6`
> Purpose: Track the current implementation batch before roadmap, architecture, diagnostics, manifest, and test documents are updated.

---

## 0. Purpose

This document records the current development iteration.

It is not a changelog.

It is not a release note.

It is not a roadmap.

It is not permanent architecture truth.

It is a temporary working note used to capture what changed during the current batch.

Permanent decisions belong in:

```txt
docs/*_ROADMAP.md
docs/DependencyGraph.md
docs/SCHEMA.md
```

This file may be rewritten for each iteration.

---

## 1. Iteration

```txt
Version: 0.0.8-dev.6
Status: Draft
Date: 2026-05-19
```

Note:

```txt
0.0.8-dev.5 skipped by explicit iteration transition request.
```

Short summary:

```txt
Added neutral SagaShared contracts and Forge BuildPlan/report foundations.
```

---

## 2. What Was Added

```txt
- Shared diagnostic severity, category, source, code, location, payload, and summary contracts.
- Unit coverage for default construction, stable vocabulary values, payload location fields, and severity summary aggregation.
- Shared artifact reference and manifest contracts.
- Shared package manifest, compatibility, dependency, and validation result contracts.
- Shared build profile/report contracts and publish readiness/report contracts.
- Unit coverage for artifact refs/manifests, package manifests/validation, build reports, and publish blockers.
- Forge BuildPlan foundation with command-to-step lowering and validation for duplicate step ids, missing dependencies, dependency cycles, duplicate output writers, and deterministic step metadata.
- Standalone Forge build plan test coverage.
- Forge BuildReport foundation that converts BuildPlan validation results into planned/blocked structured reports.
- Deterministic Forge build report JSON writer with string escaping.
- Standalone Forge build report test coverage.
- `forge plan <command> [--json]` CLI preview for BuildPlan/BuildReport output without running backend adapters.
- CTest coverage for JSON plan preview, text plan preview, and unknown plan command failure.
- `forge plan <command> --write-report` persistence for preview build reports.
- CTest coverage for text and JSON `--write-report` smoke paths.
- Minimal `forge report` reader preview for persisted build reports.
- CTest coverage for `forge report --json`, `--build=DIR --json`, `--path=FILE --json`, text hint output, and missing report failure.
- Forge-local diagnostic collector foundation.
- Standalone Forge diagnostic collector test coverage.
- Forge-local stable exit code foundation with named categories.
- Standalone Forge exit code test coverage.
```

---

## 3. What Was Changed

```txt
- docs/dev/ITERATION_NOTES.md updated with this shared contract slice.
- .gitignore now explicitly allows Shared/include/SagaShared/Build/*.hpp despite broad Build ignores.
- SHARED_ROADMAP.md, DIAGNOSTICS_ROADMAP.md, and BUILD_PUBLISH_PIPELINE_ROADMAP.md synced with the shipped shared contract slice.
- Tools/Forge/CMakeLists.txt now builds the BuildPlan foundation and registers forge_build_plan_tests.
- FORGE_ROADMAP.md synced with the shipped BuildPlan foundation slice.
- Tools/Forge/CMakeLists.txt now builds the BuildReport foundation and registers forge_build_report_tests.
- FORGE_ROADMAP.md synced with the shipped BuildReport foundation slice while keeping persisted reports and CLI report commands open.
- Tools/Forge/src/main.cpp now exposes BuildPlan report previews through `forge plan`.
- Tools/Forge/CMakeLists.txt now registers CLI smoke tests for `forge plan`.
- Tools/Forge/src/main.cpp now writes preview reports to `<build-dir>/Reports/build_report.json` when `--write-report` is used.
- Tools/Forge/CMakeLists.txt now runs `forge plan --write-report` smoke tests from build-local working directories.
- Tools/Forge/src/main.cpp now exposes a minimal `forge report` reader without parsing JSON.
- Tools/Forge/CMakeLists.txt now registers `forge report` smoke tests.
- BuildPlan validation diagnostics now flow through the Forge-local DiagnosticCollector before entering build reports.
- Tools/Forge/CMakeLists.txt now builds DiagnosticCollector and registers forge_diagnostic_collector_tests.
- Tools/Forge/src/main.cpp now routes local exit constants through Forge::ExitCode while preserving existing numeric behavior.
- Tools/Forge/CMakeLists.txt now builds ExitCode and registers forge_exit_code_tests.
```

---

## 4. What Was Removed

```txt
- Nothing removed.
```

---

## 5. Files Changed

```txt
- .gitignore
- docs/dev/ITERATION_NOTES.md
- docs/roadmaps/BUILD_PUBLISH_PIPELINE_ROADMAP.md
- docs/roadmaps/DIAGNOSTICS_ROADMAP.md
- docs/roadmaps/SHARED_ROADMAP.md
- Tools/Forge/CMakeLists.txt
- Tools/Forge/FORGE_ROADMAP.md
- Tools/Forge/include/Forge/Diagnostics/DiagnosticCollector.hpp
- Tools/Forge/include/Forge/Diagnostics/ForgeDiagnostic.hpp
- Tools/Forge/include/Forge/ExitCode.h
- Tools/Forge/include/Forge/Pipeline/BuildPlan.hpp
- Tools/Forge/include/Forge/Pipeline/BuildPlanner.hpp
- Tools/Forge/include/Forge/Reports/BuildReport.hpp
- Tools/Forge/include/Forge/Reports/BuildReportBuilder.hpp
- Tools/Forge/include/Forge/Reports/BuildReportWriter.hpp
- Tools/Forge/src/main.cpp
- Tools/Forge/src/Diagnostics/DiagnosticCollector.cpp
- Tools/Forge/src/ExitCode.cpp
- Tools/Forge/src/Pipeline/BuildPlanner.cpp
- Tools/Forge/src/Reports/BuildReportBuilder.cpp
- Tools/Forge/src/Reports/BuildReportWriter.cpp
- Tools/Forge/tests/BuildPlanTests.cpp
- Tools/Forge/tests/BuildReportTests.cpp
- Tools/Forge/tests/DiagnosticCollectorTests.cpp
- Tools/Forge/tests/ExitCodeTests.cpp
- Shared/include/SagaShared/Diagnostics/DiagnosticCategory.hpp
- Shared/include/SagaShared/Diagnostics/DiagnosticCode.hpp
- Shared/include/SagaShared/Diagnostics/DiagnosticLocation.hpp
- Shared/include/SagaShared/Diagnostics/DiagnosticPayload.hpp
- Shared/include/SagaShared/Diagnostics/DiagnosticSeverity.hpp
- Shared/include/SagaShared/Diagnostics/DiagnosticSource.hpp
- Shared/include/SagaShared/Diagnostics/DiagnosticSummary.hpp
- Shared/include/SagaShared/Artifacts/ArtifactDependency.hpp
- Shared/include/SagaShared/Artifacts/ArtifactHash.hpp
- Shared/include/SagaShared/Artifacts/ArtifactId.hpp
- Shared/include/SagaShared/Artifacts/ArtifactKind.hpp
- Shared/include/SagaShared/Artifacts/ArtifactManifest.hpp
- Shared/include/SagaShared/Artifacts/ArtifactRef.hpp
- Shared/include/SagaShared/Artifacts/ArtifactStatus.hpp
- Shared/include/SagaShared/Build/BuildConfiguration.hpp
- Shared/include/SagaShared/Build/BuildId.hpp
- Shared/include/SagaShared/Build/BuildProfile.hpp
- Shared/include/SagaShared/Build/BuildProfileId.hpp
- Shared/include/SagaShared/Build/BuildReport.hpp
- Shared/include/SagaShared/Build/BuildStatus.hpp
- Shared/include/SagaShared/Build/BuildStepKind.hpp
- Shared/include/SagaShared/Build/BuildStepResult.hpp
- Shared/include/SagaShared/Build/TargetPlatform.hpp
- Shared/include/SagaShared/Packages/PackageCompatibility.hpp
- Shared/include/SagaShared/Packages/PackageDependency.hpp
- Shared/include/SagaShared/Packages/PackageId.hpp
- Shared/include/SagaShared/Packages/PackageKind.hpp
- Shared/include/SagaShared/Packages/PackageManifest.hpp
- Shared/include/SagaShared/Packages/PackageValidationResult.hpp
- Shared/include/SagaShared/Packages/PackageVersion.hpp
- Shared/include/SagaShared/Publish/PublishBlocker.hpp
- Shared/include/SagaShared/Publish/PublishBlockerKind.hpp
- Shared/include/SagaShared/Publish/PublishReadiness.hpp
- Shared/include/SagaShared/Publish/PublishReport.hpp
- Shared/include/SagaShared/Publish/PublishStatus.hpp
- Tests/Unit/Shared/ArtifactPackageBuildContractsTests.cpp
- Tests/Unit/Shared/DiagnosticContractsTests.cpp
```

---

## 6. Ownership / Boundary Notes

```txt
Allowed:
- SagaShared owns neutral, data-only diagnostic payload contracts.
- SagaShared owns neutral, data-only artifact, package, build, and publish report contracts.

Forbidden:
- Preserve existing DependencyGraph.md ownership boundaries.
- Keep SDE standalone.
- Keep Forge as an orchestrator only.
- Keep Prism as an analyzer only.
- Keep runtime/server as manifest/artifact consumers.
- Keep editor as authoring UX only.
- Keep SagaShared implementation-free.
```

---

## 7. Diagnostics / Manifests / Reports

Diagnostics:

```txt
- Added shared diagnostic payload contracts only.
- Artifact, package, build, and publish contracts can reference diagnostic summaries or payloads.
- No diagnostic report envelope, writer, storage, editor routing, Forge aggregation, Prism output, or publish gate implementation was added.
```

Manifests:

```txt
- Added shared artifact and package manifest value contracts.
- No manifest loader, writer, serializer, package staging, or runtime migration was added.
```

Reports:

```txt
- Added shared build report and publish report value contracts.
- Added Forge-local BuildReport foundation and deterministic JSON stream writer for BuildPlan validation reports.
- Added `forge plan <command> [--json]` for CLI report preview without backend execution.
- Added `forge plan <command> --write-report` for persisted preview reports.
- Added minimal `forge report` reader preview for persisted build reports.
- Added Forge-local DiagnosticCollector foundation for build report diagnostics.
- Added Forge-local ExitCode foundation for named CLI exit categories.
- No JSON parser, JSON schema, parsed report summary, Forge tool-output aggregation, tool diagnostic normalization, command execution report emission, diagnostics report reader, publish report reader, or CI artifact emission was added.
- Forge BuildPlan foundation can now produce planned/blocked report data, but build/test command execution does not emit reports yet.
- No full command exit-code classification, CI profile behavior, package failure mapping, or publish-blocker exit behavior was added.
```

---

## 8. Verification

```txt
Command:
Tools/Forge/bin/forge nix build -j 1

Result:
Not completed

Notes:
Forge entered the Nix shell successfully but failed because build/RelWithDebInfo is not configured.
Earlier prerequisite setup entered an external Qt 6.6.2 source build at -j1 because the compatible Qt package was not cached; that external build was stopped during dependency preparation.

Focused verification:
g++ -std=c++20 -Wall -Wextra -Wpedantic -I Shared/include /tmp/saga_shared_contracts_compile.cpp -o /tmp/saga_shared_contracts_compile
/tmp/saga_shared_contracts_compile
git diff --check
rg -n "SagaEditor/|SagaServer/|SagaCollaboration/|Apps/Saga/|SDE/|<Q" Shared/include/SagaShared
rg -n "Shared/include/SagaShared/(Diagnostics|Artifacts|Packages|Build|Publish)" docs/roadmaps/SHARED_ROADMAP.md docs/roadmaps/DIAGNOSTICS_ROADMAP.md docs/roadmaps/BUILD_PUBLISH_PIPELINE_ROADMAP.md
rg -n "\[x\].*(Forge emits|writer|loader|Problems|publish gate|JSON|Generate artifact manifest|Generate package manifest|Produce publish report|Integrate with Forge reports|Emit publish report|Define report envelope)" docs/roadmaps/SHARED_ROADMAP.md docs/roadmaps/DIAGNOSTICS_ROADMAP.md docs/roadmaps/BUILD_PUBLISH_PIPELINE_ROADMAP.md
python3 Tools/Forge/build.py -j 1
cmake --build Tools/Forge/build --target forge_exit_code_tests -j 1
Tools/Forge/build/forge_exit_code_tests
cmake --build Tools/Forge/build --target forge_build_plan_tests -j 1
Tools/Forge/build/forge_build_plan_tests
ctest --test-dir Tools/Forge/build --output-on-failure -R forge_build_plan_tests
cmake --build Tools/Forge/build --target forge_build_scheduler_tests -j 1
ctest --test-dir Tools/Forge/build --output-on-failure
cmake --build Tools/Forge/build --target forge_build_report_tests -j 1
Tools/Forge/build/forge_build_report_tests
cmake --build Tools/Forge/build --target forge_diagnostic_collector_tests -j 1
Tools/Forge/build/forge_diagnostic_collector_tests
Tools/Forge/bin/forge plan build --json
Tools/Forge/bin/forge plan test
/home/arda/Desktop/Klasorler/SagaEngine\ Versions\ 2/SagaEngine\ 0.0.8/Tools/Forge/bin/forge plan build --write-report
rg -n '"status": "Planned"|"forge.report.path"' /tmp/build/Reports/build_report.json
/home/arda/Desktop/Klasorler/SagaEngine\ Versions\ 2/SagaEngine\ 0.0.8/Tools/Forge/bin/forge report --json
/home/arda/Desktop/Klasorler/SagaEngine\ Versions\ 2/SagaEngine\ 0.0.8/Tools/Forge/bin/forge report
/home/arda/Desktop/Klasorler/SagaEngine\ Versions\ 2/SagaEngine\ 0.0.8/Tools/Forge/bin/forge report --path=missing.json --json
ctest --test-dir Tools/Forge/build --output-on-failure
rg -n "BuildPlan|BuildPlanner|forge_build_plan_tests|BuildPipeline|BuildReport" Tools/Forge/FORGE_ROADMAP.md
rg -n "\[x\].*(BuildPipeline|BuildReport|JSON|Emit machine-readable|PackageStage|PublishReadinessStep|diagnostics aggregation)" Tools/Forge/FORGE_ROADMAP.md

Focused result:
Passed. The boundary token search returned no matches. The roadmap shipped-item guard returned no matches. Forge ExitCode, BuildPlan, BuildReport, and DiagnosticCollector tests passed directly and through CTest. `forge plan build --json`, `forge plan test`, and `forge plan build --write-report` produced planned report previews. The persisted preview report contained `Planned` status and `forge.report.path` metadata. `forge report --json` streamed the persisted report, text mode printed path/size/hint only, and missing report returned non-zero. Full Forge CTest passed after building test targets. Forge roadmap BuildPlan/BuildReport/DiagnosticCollector references were found, and the Forge roadmap shipped-item guard returned no matches. Existing CLI numeric behavior remains success=0, usage=1, execution failure=2, strict failure=3.

Not tested:
- Full Tools/Forge/bin/forge nix build -j 1 did not complete because build/RelWithDebInfo is not configured.
- GTest-based SagaArchitectureTests were not run for the same build setup reason.
```

---

## 9. Roadmaps To Update

```txt
[ ] EDITOR_ROADMAP.md
[x] SHARED_ROADMAP.md
[ ] ENGINE_ROADMAP.md
[ ] SDE_ROADMAP.md
[x] FORGE_ROADMAP.md
[ ] PRISM_ROADMAP.md
[ ] COLLABORATION_ROADMAP.md
[ ] TOOLS_ROADMAP.md
[ ] DependencyGraph.md
[x] DIAGNOSTICS_ROADMAP.md
[ ] ASSET_PIPELINE_ROADMAP.md
[ ] SAGA_SCRIPTING_ROADMAP.md
[x] BUILD_PUBLISH_PIPELINE_ROADMAP.md
[ ] AssetStreamingImplementation.md
[ ] SCHEMA.md
[ ] Other:
```

Reason:

```txt
Shared diagnostic, artifact, package, build, and publish contracts were added as early cross-system dependency slices.
Forge BuildPlan, BuildReport, and DiagnosticCollector foundations were added under Tools/Forge, with CLI plan preview, preview report persistence, and minimal report reading.
```

---

## 10. Known Problems

```txt
- Full engine Forge build verification currently requires a configured build/RelWithDebInfo directory and may require completing an external Qt source build for the active Conan profile.
- Forge standalone BuildPlan, BuildReport, DiagnosticCollector, plan preview, preview report persistence, and report reader preview verification passes.
```

---

## 11. Next Actions

```txt
[x] Update SHARED_ROADMAP.md, DIAGNOSTICS_ROADMAP.md, and BUILD_PUBLISH_PIPELINE_ROADMAP.md from this iteration note.
[x] Update FORGE_ROADMAP.md from the BuildPlan foundation slice.
[x] Update FORGE_ROADMAP.md from the BuildReport foundation slice.
[x] Update FORGE_ROADMAP.md from the `forge plan` preview slice.
[x] Update FORGE_ROADMAP.md from the `forge plan --write-report` persistence slice.
[x] Update FORGE_ROADMAP.md from the minimal `forge report` reader preview slice.
[x] Update FORGE_ROADMAP.md from the DiagnosticCollector foundation slice.
```

---

## 12. Roadmap Update Request

Use this block when updating roadmaps from this iteration.

```txt
Read docs/dev/ITERATION_NOTES.md.

Update only the roadmap files marked in section 9.

Rules:
- Do not rewrite unrelated sections.
- Do not mark unverified work as shipped.
- If status is Draft or Partially Implemented, keep roadmap items open.
- Preserve DependencyGraph.md ownership boundaries.
- Keep SDE standalone.
- Keep Forge as orchestrator only.
- Keep Prism as analyzer only.
- Keep runtime/server as manifest/artifact consumers.
- Keep editor as authoring UX only.
- Keep SagaShared implementation-free.
```

---

## 13. Next Iteration

```txt
Iteration: 0.0.8-dev.6

Added:
- Neutral SagaShared diagnostic payload contracts.
- Neutral SagaShared artifact and package manifest contracts.
- Neutral SagaShared build report and publish readiness/report contracts.
- Forge BuildPlan foundation with command lowering and plan validation.
- Forge BuildReport foundation with planned/blocked report data and deterministic JSON stream writer.
- `forge plan <command> [--json]` CLI preview.
- `forge plan <command> --write-report` preview report persistence.
- Minimal `forge report` reader preview.
- Forge-local DiagnosticCollector foundation.
- Forge-local stable ExitCode foundation.
- Unit coverage for shared contracts, Forge BuildPlan behavior, Forge BuildReport behavior, Forge DiagnosticCollector behavior, Forge ExitCode behavior, and Forge plan/report CLI previews.

Changed:
- Roadmaps synced for shipped shared contract and Forge BuildPlan slices.
- .gitignore adjusted so Shared/include/SagaShared/Build/*.hpp is tracked.
- Forge CMake wiring now builds BuildPlanner and registers forge_build_plan_tests.
- Forge CMake wiring now builds BuildReportBuilder/BuildReportWriter and registers forge_build_report_tests.
- Forge CMake wiring now registers `forge plan` CLI smoke tests.
- Forge CMake wiring now registers `forge plan --write-report` smoke tests.
- Forge CMake wiring now registers `forge report` smoke tests.
- Forge CMake wiring now builds DiagnosticCollector and registers forge_diagnostic_collector_tests.
- Forge CMake wiring now builds ExitCode and registers forge_exit_code_tests.
- Forge CLI exit constants now route through Forge::ExitCode while preserving existing numeric behavior.
- BuildReportBuilder now routes BuildPlan validation diagnostics through DiagnosticCollector.

Tested:
- Shared contract focused compile check.
- Shared boundary token search.
- Roadmap shipped-item guard checks.
- Forge standalone build.
- forge_exit_code_tests direct run and CTest.
- forge_build_plan_tests direct run and CTest.
- forge_build_report_tests direct run and CTest.
- forge_diagnostic_collector_tests direct run and CTest.
- `forge plan build --json`.
- `forge plan test`.
- `forge plan build --write-report`.
- `forge report --json`.
- `forge report`.
- Full Forge CTest after building test targets.
- git diff --check.

Not tested:
- Full Tools/Forge/bin/forge nix build -j 1 did not complete because build/RelWithDebInfo is not configured.
```
