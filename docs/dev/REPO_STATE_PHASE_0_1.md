# SagaEngine Phase 0-1 Repo State

Date: 2026-06-02
Status: accounting document for git hygiene and repository recovery.

This document records the current worktree shape before feature cleanup,
product-doc rewrites, tool boundary changes, or source refactors. It is meant
to make the dirty tree explainable and to define safe commit packages.

## Executive Verdict

The repository is partially explainable but not committable as a single change.
The worktree contains multiple mixed workstreams:

- diagnostics and server/source recovery;
- SagaScript public-tool expansion;
- new public-tool candidates;
- internal evidence and gate tools;
- sample fixture source;
- product, architecture, roadmap, and recovery documentation;
- CMake/test registration changes.

The first recovery commit should be an accounting and generated-output hygiene
commit only. It should not move large docs, delete user work, rename samples, or
claim product readiness.

## Baseline Counts

Captured before adding this document.

| Class | Count | Examples | Commit handling |
| --- | ---: | --- | --- |
| Modified tracked source | 13 | `Engine/.../Diagnostics/*`, `Server/.../ZoneServer.*`, `CollaborationService.hpp` | Source/test recovery package after review |
| Modified tracked docs | 4 | `docs/README.md`, `docs/dev/ITERATION_NOTES.md`, `docs/roadmaps/DIAGNOSTICS_ROADMAP.md`, `docs/testing/TEST_SUITES.md` | Separate docs package |
| Modified tracked tests | 4 | diagnostics/runtime/architecture tests | Pair with matching source package |
| Modified tracked tools | 7 | `Tools/SagaScript/*` | Public tool package candidate |
| Modified build/config | 3 | `.gitignore`, `cmake/modules/SagaTargets.cmake`, `cmake/modules/SagaTests.cmake` | Split `.gitignore` first; CMake with tests |
| Untracked source | 54 | diagnostics, collaboration, editor authoring, network chaos | Human/API review required |
| Untracked docs | 106 | `docs/architecture/*`, `docs/product/*`, new roadmaps | Product/archive taxonomy package |
| Untracked tests | 13 | diagnostics, server, editor, tools, sample tests | Pair with matching source/tool package |
| Untracked tools | 136 | `SagaProjectKit`, `SagaPackager`, gates, labs, probes | Split public and internal tools |
| Untracked sample source | 13 | `samples/MultiplayerSandbox` source files | Sample fixture package |
| Generated/build outputs | ignored | `Build/`, `build/`, `Tools/*/bin`, `Tools/*/obj`, sample `Build/`, sample `Generated/` | Do not commit |

## Generated Output Policy

Generated and build outputs are not commit candidates.

Currently ignored output areas include:

- `Build/`
- `build/`
- `Tools/*/bin/`
- `Tools/*/obj/`
- Python `__pycache__/`
- `Tools/SagaTools/target/`
- `samples/MultiplayerSandbox/Build/`
- `samples/MultiplayerSandbox/Generated/`

The `samples/MultiplayerSandbox/Generated/` ignore rule is part of the first
recovery package. The sample source files may be reviewed later, but generated
source-truth evidence must stay out of commits unless a future package
explicitly promotes a fixture artifact with a written reason.

## Recommended Commit Packages

### Package 1: Repo Accounting And Generated-Output Hygiene

Purpose: make the worktree explainable without changing product behavior.

Candidate files:

- `.gitignore`
- `docs/dev/REPO_STATE_PHASE_0_1.md`

Risk: low.

Verification:

```bash
git diff --check
git status --short --ignored
git check-ignore -v samples/MultiplayerSandbox/Generated/SourceTruth/source_truth_foundation.generated.json
git check-ignore -v samples/MultiplayerSandbox/Build/Reports/headless_server_report.json
```

Recommendation: commit first.

### Package 2: Diagnostics And Server Source Recovery

Purpose: land coherent diagnostics, health, lifetime, crash/fault, memory,
resource, server lifecycle, and ZoneServer observability work.

Candidate files:

- modified diagnostics/log/server files under `Engine/` and `Server/`;
- untracked diagnostics/server headers and sources;
- matching diagnostics and server tests.

Risk: medium-high because public headers are involved.

Verification:

```bash
ctest -R "Diagnostic|ServerLifecycle|ZoneServer" --output-on-failure
```

Recommendation: review public headers before committing.

### Package 3: CMake And Test Registration

Purpose: register the tests and targets needed by diagnostics/source recovery.

Candidate files:

- `cmake/modules/SagaTargets.cmake`
- `cmake/modules/SagaTests.cmake`
- matching test files under `Tests/Unit/`

Risk: medium.

Verification:

```bash
cmake --build build/RelWithDebInfo-0.0.9 --target DiagnosticFoundationTests -- -j2
ctest -R "DiagnosticFoundationTests" --output-on-failure
```

Recommendation: commit with or immediately after Package 2.

### Package 4: SagaScript Public Tool Recovery

Purpose: land the SagaScript expansion as one reviewable public-tool package.

Candidate files:

- `Tools/SagaScript/README.md`
- `Tools/SagaScript/SagaScript.csproj`
- `Tools/SagaScript/src/*`
- `Tools/SagaScript/tests/*`

Risk: medium.

Verification:

```bash
nix-shell --run "python3 Tools/SagaScript/tests/test_sagascript_cli.py"
```

Recommendation: commit separately from engine/source changes.

### Package 5: Public Tool Candidates

Purpose: add user-facing project/package/probe tools without mixing them with
internal gates.

Candidate files:

- `Tools/SagaProjectKit/`
- `Tools/SagaPackager/`
- `Tools/SagaProbe/`

Risk: medium. `SagaProjectKit` previously showed a failing/inconclusive
phase-evidence test path and must be fixed or explicitly split.

Verification:

```bash
nix-shell --run "dotnet build Tools/SagaProjectKit/SagaProjectKit.csproj --configuration Release"
nix-shell --run "dotnet build Tools/SagaPackager/SagaPackager.csproj --configuration Release"
nix-shell --run "python3 Tools/SagaPackager/tests/test_sagapack_cli.py"
```

Recommendation: delay until test behavior is clean.

### Package 6: Internal Evidence Tools

Purpose: preserve useful internal gates without making them look like product
CLIs.

Candidate files:

- `Tools/SagaDocGuard/`
- `Tools/SagaAlphaGate/`
- `Tools/SagaEnterpriseGate/`
- `Tools/SagaPreviewGate/`
- `Tools/SagaPolicyKit/`
- `Tools/SagaLaunchLab/`
- `Tools/SagaViewKit/`
- `Tools/SagaWorkspaceHub/`
- C++ internal labs such as `SagaChaosLab`, `SagaStressArena`, and
  `SagaStateCheck`

Risk: high product-perception risk.

Verification: build and run each tool's focused test suite.

Recommendation: delay until docs mark these as internal/evidence-only.

### Package 7: MultiplayerSandbox Fixture Source

Purpose: track only the source-controlled fixture, not generated evidence or
build reports.

Candidate files:

- `samples/MultiplayerSandbox/README.md`
- `samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj`
- `samples/MultiplayerSandbox/Scenes/`
- `samples/MultiplayerSandbox/Scripts/`
- `samples/MultiplayerSandbox/Assets/`
- `samples/MultiplayerSandbox/.saga/`
- launch and package profile JSON files

Risk: medium because the sample name can overpromise playable multiplayer.

Verification:

```bash
nix-shell --run "Tools/SagaProjectKit/sagaproject validate --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj"
```

Recommendation: delay until README/name framing is reviewed.

### Package 8: Product Docs Cleanup

Purpose: keep product-facing documentation honest and short.

Candidate files:

- `README.md`
- `docs/README.md`
- `docs/product/WHAT_IS_SAGAENGINE.md`
- `docs/product/CURRENT_CAPABILITIES.md`
- `docs/product/WHAT_IS_NOT_IMPLEMENTED.md`
- `docs/product/CURRENT_DISTRIBUTION_STATUS.md`
- `docs/product/GETTING_STARTED.md`

Risk: medium.

Verification:

```bash
rg -n "Hedef|hedef|74%|Product Beta|Release Candidate|production-ready|evidence matrix|ReportOnly" README.md docs/product
```

Recommendation: Phase 2, after git hygiene package lands.

### Package 9: Archive/Internal Docs Taxonomy

Purpose: move or reclassify phase/recovery/evidence docs so they do not read as
the product surface.

Candidate files:

- phase-numbered product docs;
- `docs/architecture/*PHASE*`;
- `docs/architecture/HEDEF*`;
- old target and enterprise roadmaps.

Risk: high because links and historical references may break.

Verification: link/search pass after moves.

Recommendation: plan and execute as a dedicated docs taxonomy batch, not during
Phase 0-1.

### Package 10: SDK Boundary Review

Purpose: prevent accidental public SDK expansion.

Candidate files:

- untracked `Engine/Public/...` diagnostics headers;
- untracked `Editor/include/...` authoring headers;
- untracked `Server/include/...` network chaos headers;
- public headers exposing Qt, SDL, or Diligent names.

Risk: high.

Verification:

```bash
rg -n "QWidget|QObject|Q_OBJECT|Diligent|SDL|ClientHost|Apps/" Engine/Public Runtime/include Server/include Editor/include Collaboration/include
```

Recommendation: delay until source packages are reviewed.

## Product-Facing Risk Notes

Do not treat current product docs as release-ready. Known risk areas include:

- `README.md` still contains an unsafe `74%` maturity-style claim.
- `docs/product/` contains phase/closure/evidence documents mixed with current
  product docs.
- `docs/roadmaps/` contains active-looking old target, phase, alpha, and
  enterprise roadmaps.
- `Hedef`, `Phase`, `closure`, `evidence matrix`, and `ReportOnly` language is
  still visible in product-adjacent paths.

These are Phase 2-4 cleanup targets, not Package 1 changes.

## First Commit Candidate

The first professional recovery commit should contain only:

- `.gitignore`
- `docs/dev/REPO_STATE_PHASE_0_1.md`

Suggested commit message:

```text
Document phase 0-1 repository state
```

Do not include source, tools, tests, product docs, or sample source in the first
commit.
