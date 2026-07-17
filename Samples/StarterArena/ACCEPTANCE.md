# StarterArena source and tooling acceptance

From the repository root, validate the project and create a fresh template with:

```text
Tools/SagaProjectKit/sagaproject validate --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter-project.json
Tools/SagaProjectKit/sagaproject template-plan --destination /tmp/StarterArenaCopy --display-name StarterArenaCopy --out /tmp/starter-plan.json
Tools/SagaProjectKit/sagaproject template-create --plan /tmp/starter-plan.json --sagapack Tools/SagaPackager/sagapack --engine-root . --runtime-bridge build/RelWithDebInfo-0.0.11/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll --out /tmp/starter-create.json
Tools/SagaPackager/sagapack resolve --project Samples/StarterArena/StarterArena.sagaproj --engine-root . --lock /tmp/SagaPackage.lock --generated-out /tmp/starter-packages --runtime-bridge build/RelWithDebInfo-0.0.11/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll --out /tmp/starter-resolve.json
Tools/SagaPackager/sagapack lock-verify --project Samples/StarterArena/StarterArena.sagaproj --engine-root . --lock Samples/StarterArena/SagaPackage.lock --generated-out /tmp/starter-packages --runtime-bridge build/RelWithDebInfo-0.0.11/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll --out /tmp/starter-lock.json
Tools/SagaPackager/sagapack compile-assets --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter-content
Tools/SagaPackager/sagapack compile-inputs --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter-content
Tools/SagaScript/sagascript analyze --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter-analysis
Tools/SagaScript/sagascript compile --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter-legacy-manifests --artifacts-out /tmp/starter-legacy-artifacts
Tools/SagaScript/sagascript project-blocks --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter-projection
Tools/SagaScript/sagascript worker --request /tmp/starter-worker-request-v1.json --result /tmp/starter-worker-result-v1.json --progress /tmp/starter-worker-progress.ndjson
Tools/SagaPackager/sagapack compile-scenes --project Samples/StarterArena/StarterArena.sagaproj --script-manifest /tmp/starter-scripts/manifests/current.json --out /tmp/starter-content
sha256sum /tmp/SagaPackage.lock /tmp/starter-content/manifests/current.json /tmp/starter-scripts/manifests/current.json
```

The checked lock is the exact offline graph for `com.saga.gameplay.foundation`
and `gameplay.highlevel`. Template creation regenerates and verifies that lock
inside its staging transaction, so the new project passes offline lock verification
immediately. Later commands in this campaign cook
asset/scene/input artifacts and compile scripts through the one-shot worker.
Selected-generation verification requires the tracked and immutable-generation
lock to be canonically identical and validates every generation file, reference
manifest, and copied RuntimeBridge hash before accepting current state.
All generated output roots must be clean or disposable, and repeating the
sequence in two roots must yield identical canonical artifacts after declared
project substitutions are normalized.

The worker request is schema version `1` and carries the selected project and
source hashes, the hashes and local paths of `/tmp/SagaPackage.lock`,
the selected package generation's C# reference and API surface manifests, plus
the exact RuntimeBridge descriptor and copied assembly hashes, the operation/revision identity,
the `/tmp/starter-scripts` output root, and an operation-matched cancellation
marker path. `python3 Tools/SagaScript/tests/test_starter_arena_conformance.py`
constructs that request and executes this complete sequence twice in separate
clean roots, then compares lock, content, assembly, manifest, source-map, and
projection bytes.
