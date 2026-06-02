# Packaging / Publish Proof Phase 29-33

> Block E evidence for Target 1 / Technical Preview Technical Preview product work.

Phase 29-33 adds `Tools/SagaPackager`, a standalone CLI for the first
MultiplayerSandbox package proof. The accepted path is server/headless only.
Client packaged launch remains deferred until a bounded ClientHost package
preview/report seam exists.

## Implemented Path

The supported commands are:

```text
sagapack validate
sagapack stage
sagapack publish-check
sagapack smoke
```

`validate` checks `.sagaproj` package profile inputs. `stage` creates a
deterministic package folder under `Build/Packages`. `publish-check` checks
local evidence inputs. `smoke` launches `MultiplayerSandboxHeadless` from the
staged package folder with a strict timeout.

## Package Profile v0

`samples/MultiplayerSandbox/package_profiles.json` defines:

- `schemaVersion`;
- profile id, display name, and role;
- package output, reports, and diagnostics directories;
- runtime package manifest path;
- asset, identity, script metadata, and runtime metadata paths;
- launch profile reference.

All profile paths must be relative and non-escaping.

## Staged Package Shape

The server/headless package contains:

```text
Project/
Manifests/
Reports/
Diagnostics/
package_manifest.server.json
package_stage_manifest.json
```

`Project/` contains copied project truth files. `Manifests/` contains empty
runtime-compatible asset/identity manifests and explicit metadata placeholders.
The package manifest uses runtime package schema version 1.

## Evidence Gate

`publish-check` requires:

- valid project/profile evidence;
- passed stage report;
- staged package manifest;
- diagnostics summary with no critical diagnostics;
- launch profile reference.

Script metadata is recognized when present in the staged package, but it is not
required for the server/headless profile.

## Packaged Smoke

`sagapack smoke` runs `MultiplayerSandboxHeadless` against the staged project
copy and writes a package smoke report plus the headless diagnostics report.
The client packaged launch stage is recorded as deferred.

## Non-Claims

This phase does not provide a setup bundle, binary delta updater, certificate
seal flow, archive packing, marketplace upload, hosted deployment, commercial
launch path, C# gameplay proof, node authoring proof, production server proof,
production network proof, or broad package pipeline readiness.
