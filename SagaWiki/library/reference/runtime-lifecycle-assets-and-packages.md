---
title: Runtime lifecycle, assets, and packages
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Runtime lifecycle, assets, and packages

This reference describes how a SagaEngine runtime process is assembled, which project artifacts it may consume, and where validation ends and live resource work begins. It preserves the durable parts of the 0.0.10 runtime and package documentation while using the post-cutover 0.0.11 owners.

## Composition boundary

`SagaRuntime` is the runtime composition owner. Domain behavior remains in the individual modules below `Engine/Source/Runtime`; the composition layer selects implementations, orders startup, wires dependencies, owns the process-level lifetime, and reports failure. It must not become a second implementation home for rendering, persistence, networking, scripting, or world behavior.

Programs are thinner still. `Engine/Source/Programs/SagaRuntime` provides an executable entry point. It may parse arguments, resolve the selected project, construct runtime configuration, invoke composition, and translate a terminal result into a process exit code. Reusable startup policy belongs in the owning runtime module rather than in `main.cpp`.

The same distinction applies to `SagaEditor`, `SagaSandbox`, and other programs that host a runtime view. A program can choose a bounded workflow, but it does not acquire ownership of the runtime modules it hosts.

## Project input

The canonical project input is a validated `.sagaproj` manifest. Schema version `0` is the current manifest contract. SagaProjectKit validates that input; SagaPackager consumes the same contract for package work. A sample manifest is an example and evidence fixture, not a competing schema authority.

Runtime startup should receive resolved, validated project data rather than search the repository for plausible content. At minimum the composition path needs enough information to identify:

- the project and its manifest version;
- the selected runtime/build profile;
- the content roots or package roots permitted by that profile;
- the startup scene or equivalent entry artifact when one is declared;
- script and artifact references that must be checked before use;
- diagnostics destinations and bounded runtime configuration.

A path present in JSON is not automatically trusted. Path normalization and project-boundary checks occur before the runtime treats it as a file. Absolute paths, traversal outside the project root, ambiguous aliases, and missing required inputs must produce deterministic validation failure rather than a fallback search.

## Ordered startup

The exact implementation can evolve, but the dependency order is stable:

1. Establish process diagnostics early enough to report later startup failures.
2. Load and validate the project manifest and selected configuration.
3. Validate package and artifact manifests before resolving payloads.
4. Establish asset identity, virtual content access, and runtime registries.
5. Initialize low-level platform, input, audio, and graphics services required by the selected workflow.
6. Initialize higher-level rendering, UI, scripting, simulation, networking, replication, persistence, world, or authority owners that the workflow enables.
7. Load the declared startup artifacts through their owning loaders.
8. Enter the runtime loop only after required owners report ready.

Optional modules are not silently treated as successful merely because they were not built or configured. Configuration distinguishes not requested, available, and requested-but-unavailable states where that distinction matters. A headless or test workflow may intentionally omit graphics; a graphical workflow must not reinterpret a graphics startup failure as a headless request.

Startup is transactional at the process level: if owner `N` fails, the process does not continue into the frame loop with a partially initialized graph. Already-started owners are shut down in reverse dependency order, and the primary failure remains visible even if cleanup also reports diagnostics.

## Runtime loop ownership

The composition layer owns the high-level loop and delegates domain work. A typical frame includes platform/input collection, fixed or variable simulation work according to the configured policy, world and replication updates, script callbacks, render snapshot construction, render submission, UI, diagnostics, and presentation. This is an ownership sequence, not a promise that every module participates in every workflow.

Normal graphics flow is `BeginFrame`, record/submit work, and `EndFrame` or present through the intended Render/RHI boundary. It must not perform a global native `WaitForIdle` on every frame. Full idle waits are reserved for teardown, exceptional recovery, or explicitly diagnostic capture paths where their cost and semantic impact are understood.

The simulation state remains authoritative for behavior. Render snapshots and interpolated client views are derived presentation state. Editor selection, panel state, and preview controls are host state. Keeping these truths separate avoids a frame-order convenience becoming an unintended authority transfer.

## Ordered shutdown

Shutdown reverses the dependency graph. New external work is stopped before the owners of that work disappear. Network intake, script callbacks, streaming submissions, and frame production are quiesced before their dependencies are destroyed. GPU resources are released before the native device lifecycle ends. Diagnostic sinks stay available long enough to record cleanup failures.

Shutdown should be idempotent at each owner boundary. A startup failure may invoke cleanup on an owner that only partially initialized; a normal program exit may encounter a module already stopped by an earlier fatal condition. Idempotence does not mean suppressing errors. It means repeated cleanup does not double-free, resubmit work, or invent a successful state.

## Manifest layers

SagaEngine uses several related but distinct records:

| Record | Purpose | Current owner |
| --- | --- | --- |
| `.sagaproj` | Project identity, schema version, project-relative entry references | SagaProjectKit contract consumed by tools/runtime |
| Asset manifest | Stable asset key, kind, runtime/cooked path, and identity inputs | Runtime `Assets` contract |
| Artifact manifest | Produced artifact identity, kind, hash, dependencies, and freshness | Runtime `Assets` contract |
| Package manifest | Package identity, version, kind, dependencies, and compatibility | Runtime `Assets` contract |
| Runtime registry | Efficient mapping used by active resource loading | Runtime `Resources` owner |
| Publish or preflight report | Derived readiness evidence and blockers | SagaPackager/tooling output |

These records must not be collapsed into one vaguely named manifest. The project manifest says what project is being opened. An asset manifest describes content identities. An artifact manifest describes produced output and its provenance. A package manifest describes distribution composition. The registry is an in-memory runtime representation, not source truth.

## Asset manifest loading

`Engine/Source/Runtime/Assets` owns the public manifest values and loaders. A load operation should separate I/O, parse, structural validation, semantic validation, and registration. Its result must carry diagnostics rather than forcing the caller to infer failure from an empty vector.

A valid entry has a stable key, a recognized kind, and a path representation allowed by the manifest contract. The loader rejects malformed JSON, duplicate keys or identifiers, invalid kinds, unsafe paths, missing required fields, and values outside declared bounds. When validation needs the manifest location to resolve a relative path, that base is explicit in load options or the caller's setup.

The manifest is not permission to glob the content tree at runtime. Discovery belongs to an authoring, inventory, or cooking workflow. Runtime consumption follows explicit entries so that two machines do not choose different files because their working directories contain different leftovers.

## Identity and registration

Stable identity is resolved before live streaming. `AssetIdentityManifest`, `AssetIdResolver`, and `AssetManifestRegistryAdapter` connect validated manifest keys to runtime `AssetId` values and registry entries. The adapter can plan entries before mutation, which permits collision and path checks to complete before the active registry changes.

Registration should be all-or-diagnosed. A report records the manifest path, source index, asset key, resolved identifier, and resolved runtime path where available. Duplicate identity, a collision with an existing registry entry, an invalid path, or a missing mapping is explicit. A partial mutation must not be presented as successful full registration.

The registry is a runtime lookup table rather than an authoring database. It carries what loaders need: identity, kind, source or virtual path, flags, approximate size, and relevant LOD information. Thumbnails, editor selection, review comments, and arbitrary import settings remain with their proper authoring owners.

## Artifact startup validation

An artifact is accepted because its manifest and bytes agree, not because a file with the expected extension exists. Startup validation can check:

- artifact kind and schema compatibility;
- declared hash and actual payload hash;
- required dependencies and their compatible versions;
- source/input fingerprints when freshness is required;
- project or package boundary rules;
- whether a required artifact is missing, stale, or generated by an incompatible tool contract.

Failure policy depends on the artifact role. A required startup artifact blocks entry into the runtime loop. An optional artifact can be reported unavailable if the owning consumer has an explicit fallback. Silent regeneration or source mutation is not a runtime fallback. A developer workflow may invoke the owning generator separately and then retry validation.

## Package startup validation

Package validation confirms identity, version, kind, dependencies, and compatibility before package content is mounted or consumed. It does not prove that every file inside a package is safe or that all package code paths are production ready. It is one contract boundary in a larger startup sequence.

Dependency evaluation is deterministic. Missing required packages, incompatible versions, cycles where forbidden, duplicate identities, or inconsistent target/platform declarations produce structured blockers. Optional dependencies remain explicitly optional; they are not silently promoted to required because one machine happened to have them installed.

Package paths are logical inputs to a content access boundary. A package archive, unpacked staging tree, memory fixture, or native directory can provide bytes through the selected source/VFS implementation without changing the public asset identity.

## Authoring versus runtime consumption

Importing source art, generating thumbnails, editing scenes, compiling scripts, and cooking platform-specific payloads are authoring/build responsibilities. Runtime modules consume validated output. The durable direction is:

```text
project source and authoring metadata
  -> validation/import/cook tools
  -> manifests plus generated artifacts
  -> startup validation
  -> runtime registries and sources
  -> domain consumers
```

The runtime may support loose-file development inputs, but the boundary remains the same: identity and validation precede consumption. Loose-file mode must not become an implicit promise that arbitrary source formats are decoded inside a shipped runtime.

## Development reload

Reload can be useful in an editor or development program, but it is not an exemption from freshness and lifetime rules. A reload request identifies the source version it observed, prevents new consumers from receiving an invalid transitional object, and replaces the resident value only after the new value validates. Existing borrows either remain valid until released or follow a documented invalidation protocol.

Runtime reload must not write back to the authoritative source merely to reconcile a derived cache. Visual Blocks patching, scene edits, and manifest edits use their own semantic transaction and apply policies. A file watcher can request revalidation; it does not become the author of the file.

## Diagnostics

Startup and content diagnostics should answer:

- which project, profile, manifest, package, or artifact was examined;
- which owner emitted the result;
- what stable diagnostic identifier classifies the issue;
- whether the issue is missing, malformed, incompatible, stale, unsafe, unavailable, or internally failed;
- whether the runtime stopped, disabled an optional capability, or continued with a declared fallback;
- which paths and identifiers are safe to report without dumping sensitive content.

Do not replace these fields with a single `false` return or an unstructured exception string. Human-readable messages are useful, but stable identifiers and bounded context make tests and tooling dependable.

## Evidence boundary

Manifest parser tests prove accepted and rejected record shapes. Path-boundary tests prove traversal and absolute-path policy. Startup-validator tests prove freshness, dependency, and compatibility decisions. Runtime smoke tests prove one configured composition reaches and exits its loop. Package/distribution tests prove staged artifacts can be found in their packaged layout.

None of those alone proves a finished public SDK, arbitrary project compatibility, production persistence, a shipped dedicated server, or durable persistent worlds. The claim must stay no broader than the test and environment that produced the evidence.

## Review checklist

When changing runtime startup or package consumption:

- name the module that owns the new decision;
- keep entry points thin and reusable behavior module-owned;
- validate paths before I/O and manifests before registration;
- distinguish project, asset, artifact, package, and registry records;
- preserve deterministic failure and reverse-order cleanup;
- do not discover content by repository glob in the runtime;
- do not regenerate or mutate authoritative source silently;
- add focused evidence for each newly accepted and rejected case;
- state optional fallback separately from required startup success.
