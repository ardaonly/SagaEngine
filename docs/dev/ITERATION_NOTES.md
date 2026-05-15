````md
# Iteration Notes

> Status: Active development note
> Current iteration: `0.0.8-1`
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
````

This file may be rewritten for each iteration.

---

## 1. Iteration

```txt
Version: 0.0.8-1
Status: Draft
Date: 2026-05-16
```

Short summary:

```txt
0.0.8-dev.1 runtime artifact manifest loader MVP.
```

---

## 2. What Was Added

```txt
- Added runtime artifact manifest value types under Engine/Public/SagaEngine/Artifacts.
- Added ArtifactManifestLoader for minimal JSON manifest loading and validation.
- Added Runtime.Artifact.* structured loader diagnostics.
- Added artifact manifest fixtures and focused unit coverage.
- Added ArtifactManifestLoadOptions for optional referenced artifact file validation.
- Added focused ArtifactManifestLoader coverage for malformed JSON fields and artifact entries.
- Added ArtifactStartupValidator for runtime startup artifact manifest acceptance policy.
- Added focused ArtifactStartupValidator coverage for startup success, loader failures, duplicate ids, path policy, and missing referenced files.
```

Use this section for newly added systems, files, modules, commands, manifests, reports, diagnostics, contracts, or tests.

Example:

```txt
- Added initial ArtifactManifestLoader.
- Added Runtime.Artifact.* diagnostics.
- Added Build/Manifests/sde_artifacts.example.json fixture.
```

---

## 3. What Was Changed

```txt
- Adapted the requested artifact manifest loader paths to the repository's enforced Engine/Public and Engine/Private layout.
- Kept nlohmann_json private to the loader implementation instead of exposing it through public headers.
- Changed ArtifactManifestLoader to optionally validate referenced artifact file existence without changing default LoadFromFile behavior.
- Changed ArtifactManifestLoader diagnostics to distinguish missing fields from invalid field types or shapes.
- Changed runtime artifact startup validation to resolve artifact references relative to the manifest parent directory as the MVP artifact root.
```

Use this section for modified behavior.

Example:

```txt
- Changed runtime startup to validate package manifests earlier.
- Changed asset diagnostics to include package/artifact context.
```

---

## 4. What Was Removed

```txt
- Nothing removed.
```

Use this section for deleted behavior, files, shortcuts, old assumptions, or deprecated paths.

Example:

```txt
- Removed direct source asset fallback from shipping runtime path.
```

---

## 5. Files Changed

```txt
Engine/Public/SagaEngine/Artifacts/ArtifactKind.hpp
Engine/Public/SagaEngine/Artifacts/ArtifactRef.hpp
Engine/Public/SagaEngine/Artifacts/ArtifactManifest.hpp
Engine/Public/SagaEngine/Artifacts/ArtifactManifestLoader.hpp
Engine/Public/SagaEngine/Artifacts/ArtifactStartupValidator.hpp
Engine/Private/SagaEngine/Artifacts/ArtifactManifestLoader.cpp
Engine/Private/SagaEngine/Artifacts/ArtifactStartupValidator.cpp
Engine/tests/fixtures/artifacts/valid_artifact_manifest.json
Engine/tests/fixtures/artifacts/invalid_missing_path_manifest.json
Tests/Unit/Runtime/ArtifactManifestLoaderTests.cpp
Tests/Unit/Runtime/ArtifactStartupValidatorTests.cpp
docs/dev/ITERATION_NOTES.md
```

---

## 6. Ownership / Boundary Notes

```txt
Allowed:
- Runtime/server may read artifact manifests as manifest/artifact consumers.
- Runtime artifact manifest loading may validate manifest shape, schema version, required fields, and artifact kind tokens.
- Runtime artifact manifest loading may optionally validate that referenced artifact files exist.
- Runtime artifact manifest diagnostics may classify malformed existing fields separately from missing fields.
- Runtime startup validation may apply runtime acceptance policy after loader parsing succeeds.
- Runtime startup validation may reject duplicate artifact ids, empty ids or paths, absolute paths, escaping relative paths, and missing referenced files.

Forbidden:
- Runtime/server must not include SDE compiler internals.
- Runtime/server must not include Forge package staging internals.
- Runtime/server must not include Prism analysis internals.
- Runtime artifact manifest loading must not compile, cook, stage, or analyze artifacts.
- Runtime artifact manifest loading must not validate hashes yet.
- Runtime startup validation must not add package loading, Forge/SDE/Prism integration, editor UI, or runtime boot orchestration.
```

Example:

```txt
Allowed:
Runtime reads artifact manifest files.

Forbidden:
Runtime includes SDE compiler internals.
Forge owns asset cooker internals.
Prism regenerates stale artifacts.
```

If nothing changed:

```txt
No new ownership boundary.
```

---

## 7. Diagnostics / Manifests / Reports

Diagnostics:

```txt
- Runtime.Artifact.ManifestMissing
- Runtime.Artifact.ParseFailed
- Runtime.Artifact.UnsupportedVersion
- Runtime.Artifact.MissingField
- Runtime.Artifact.InvalidField
- Runtime.Artifact.UnknownKind
- Runtime.Artifact.FileMissing
- Runtime.Artifact.DuplicateId
```

Manifests:

```txt
- Engine/tests/fixtures/artifacts/valid_artifact_manifest.json
- Engine/tests/fixtures/artifacts/invalid_missing_path_manifest.json
```

Reports:

```txt
- No report format added.
```

Example:

```txt
Diagnostics:
- Runtime.Artifact.ManifestMissing
- Runtime.Artifact.UnsupportedVersion

Manifests:
- Build/Manifests/sde_artifacts.json

Reports:
- Build/Reports/runtime_diagnostics.json
```

---

## 8. Verification

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the runtime artifact startup validator continuation.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=ArtifactStartupValidatorTests.*:ArtifactManifestLoaderTests.*

Result:
Passed

Notes:
33 runtime artifact tests passed: 23 ArtifactManifestLoaderTests and 10 ArtifactStartupValidatorTests.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaArchitectureTests --jobs=1

Result:
Passed

Notes:
Built architecture boundary tests through Forge/Nix with serialized build parallelism.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaArchitectureTests

Result:
Passed

Notes:
11 architecture/boundary tests passed.
```

```txt
Command:
Full ctest suite

Result:
Not run

Notes:
Only targeted runtime artifact and architecture boundary verification were run.
```

Example:

```txt
Command:
ctest -R ArtifactManifestLoader

Result:
Passed

Notes:
Valid manifest and missing file cases covered.
```

If not tested:

```txt
Command:
Not run

Result:
Not verified

Notes:
Implementation exists but was not tested yet.
```

---

## 9. Roadmaps To Update

```txt
[ ] EDITOR_ROADMAP.md
[ ] SHARED_ROADMAP.md
[ ] ENGINE_ROADMAP.md
[ ] SDE_ROADMAP.md
[ ] FORGE_ROADMAP.md
[ ] PRISM_ROADMAP.md
[ ] COLLABORATION_ROADMAP.md
[ ] TOOLS_ROADMAP.md
[ ] DependencyGraph.md
[ ] DIAGNOSTICS_ROADMAP.md
[ ] ASSET_PIPELINE_ROADMAP.md
[ ] SAGA_SCRIPTING_ROADMAP.md
[ ] BUILD_PUBLISH_PIPELINE_ROADMAP.md
[ ] AssetStreamingImplementation.md
[ ] SCHEMA.md
[ ] Other:
```

Reason:

```txt
No roadmap files were updated in this task by request. Runtime artifact manifest loading remains a small MVP slice; roadmap status can be updated in a dedicated roadmap pass if desired.
```

---

## 10. Known Problems

```txt
- Artifact hash validation is not implemented yet.
- The loader does not integrate with a shared diagnostics payload/report system yet.
- Runtime package loading and package-root policy beyond the manifest parent directory are not implemented yet.
- Runtime boot orchestration is not wired to ArtifactStartupValidator yet.
- Full ctest suite was not run.
```

Example:

```txt
- Manifest version mismatch is not handled yet.
- Runtime.Artifact.* diagnostics exist but are not shown in editor preview yet.
- Tests do not cover invalid JSON manifest input.
```

---

## 11. Next Actions

```txt
[ ] Add artifact hash validation once hash semantics are defined.
[ ] Integrate ArtifactStartupValidator with runtime startup once boot orchestration owns the call site.
[ ] Integrate artifact file validation with package startup once package roots are defined.
[ ] Promote shared artifact/diagnostic contracts only after multiple ownership domains consume them.
[ ] Consider shared diagnostic payloads after runtime/server/tool consumers need common reporting.
```

Example:

```txt
[ ] Add unsupported manifest version test.
[ ] Add ArtifactRef shared contract if multiple modules consume it.
[ ] Update ENGINE_ROADMAP.md after verification.
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
Next iteration: 0.0.8-dev.1

Possible focus:
- Runtime artifact hash and version validation.
- Runtime boot orchestration hookup for ArtifactStartupValidator.
- Runtime package manifest startup validation.
- Shared artifact manifest contract review if runtime/server/tools require the same public shape.
```

````
