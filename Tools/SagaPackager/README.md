# SagaPackager

SagaPackager is the local package proof CLI.

It validates `.sagaproj` package profiles, stages a deterministic local package
folder, checks required evidence, and runs the accepted server/headless smoke.

```sh
Tools/SagaPackager/sagapack validate --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --out /tmp/package_report.json
Tools/SagaPackager/sagapack asset-validate --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out /tmp/asset_workflow_report.json
Tools/SagaPackager/sagapack profile-matrix --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out /tmp/package_profile_matrix_report.json
Tools/SagaPackager/sagapack stage --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --out /tmp/package_report.json
Tools/SagaPackager/sagapack publish-check --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --package-report /tmp/package_report.json --diagnostics-summary /tmp/diagnostics_summary.json --out /tmp/publish_report.json
Tools/SagaPackager/sagapack publish-check --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --package-report /tmp/package_report.json --diagnostics-summary /tmp/diagnostics_summary.json --policy-report /tmp/policy_evaluation_report.json --review-approval-report /tmp/review_approval_report.json --audit-report /tmp/audit_report.json --restricted-export-report /tmp/restricted_export_report.json --out /tmp/publish_report.json
Tools/SagaPackager/sagapack smoke --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --package-report /tmp/package_report.json --out /tmp/package_smoke_report.json --timeout-sec 5 --bin-dir build/RelWithDebInfo-0.0.11/bin
Tools/SagaPackager/sagapack compile-assets --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter-content
Tools/SagaPackager/sagapack compile-scenes --project Samples/StarterArena/StarterArena.sagaproj --script-manifest /tmp/starter-scripts/manifests/current.json --out /tmp/starter-content
Tools/SagaPackager/sagapack compile-inputs --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter-content
Tools/SagaPackager/sagapack compile-content --project Samples/StarterArena/StarterArena.sagaproj --script-manifest /tmp/starter-scripts/manifests/current.json --out /tmp/starter-content
Tools/SagaPackager/sagapack resolve --project Samples/StarterArena/StarterArena.sagaproj --engine-root . --lock /tmp/SagaPackage.lock --generated-out /tmp/starter-packages --runtime-bridge build/RelWithDebInfo-0.0.11/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll --out /tmp/package-resolve.json
Tools/SagaPackager/sagapack lock-verify --project Samples/StarterArena/StarterArena.sagaproj --engine-root . --lock Samples/StarterArena/SagaPackage.lock --generated-out /tmp/starter-packages --runtime-bridge build/RelWithDebInfo-0.0.11/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll --out /tmp/package-lock-verify.json
```

The tool is project-local and file-system deterministic. `asset-validate` and
`profile-matrix` are read-only report commands. Missing accepted asset source
truth is reported as `MissingSourceOfTruth`. The tool does not build shipping
distribution artifacts, upload anything, migrate ClientHost, or change
runtime/server/editor gameplay code.

The Campaign 1 compiler accepts strict schema-version-1 primitive asset, scene,
and input sources. It emits canonical UTF-8/LF JSON with lowercase SHA-256
identities, validates the complete operation before publication, stages
content-addressed objects, and atomically replaces `manifests/current.json`.
Runtime-facing records use logical ids and hashes only. Render owns the neutral
mesh/material artifact loaders and maps them into the existing `MeshAsset`,
`MaterialRuntime`, and `renderer.solid` path.

`resolve` is deliberately offline and built-in-only. It accepts exact engine
version intents, resolves artifacts exclusively from the trusted engine
distribution, and publishes a schema-version-1 lock, C# reference, API-surface,
Visual Blocks surface metadata, and verified RuntimeBridge copy as one immutable
generation selected by `manifests/current.json`. Lock records use logical
distribution-relative paths; absolute paths are limited to local reports.
`lock-verify` rejects stale intent, missing or changed artifacts, unsupported
modules, version mismatches, trust failures, and provenance gaps. A package
copied into a project cannot supply an executable analyzer or managed reference.
When `--generated-out` is supplied, verification also validates the selected
current/generation manifests, containment and every immutable file hash, the
copied RuntimeBridge, reference metadata, and canonical equality between the
tracked and generation lock. Resolution snapshots verified RuntimeBridge bytes,
revalidates their source immediately before publication, and writes that exact
snapshot so a concurrent binary change cannot replace current state.
Scene compilation consumes hash-verified SagaScript semantic evidence; it does
not implement a second regex or Roslyn behavior analyzer.

`publish-check` accepts optional, precomputed governance reports. These reports
are external inputs produced outside SagaPackager; SagaPackager consumes and
validates them but does not produce them. In particular, this repository does
not bundle a SagaPolicyKit workflow. The `--policy-report` option remains an
optional input contract, not a claim that a policy-report producer ships with
SagaPackager.

When supplied, governance reports add report-only gates for policy, review
approval, audit evidence, and restricted export evidence. Missing, malformed,
denied, review-required, or blocked supplied evidence blocks the check;
omitting these inputs preserves the existing publish-check behavior.
