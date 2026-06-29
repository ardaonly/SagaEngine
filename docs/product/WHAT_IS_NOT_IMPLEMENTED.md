# What Is Not Implemented

> **Owner:** Product documentation, validated by subsystem owners
> **Audience:** Users, contributors, and evaluators checking non-claims
> **Update trigger:** A listed non-claim gains accepted end-to-end evidence or a new overclaim risk appears
> **Authority:** Current non-claim boundary; removal requires accepted evidence

> Status: Current product non-claims
> Scope: Capabilities that are not established as complete by current evidence

This page prevents product direction, planning documents, focused tests, and
generated reports from being presented as finished SagaEngine capabilities.

A subsystem may contain useful foundations while still appearing here. The
absence of a complete product claim does not mean that no code exists.

## Runtime And Gameplay

Current evidence does not establish a complete:

- project create, edit, run, save, build, and package user journey;
- playable standalone runtime product loop;
- complete input, physics, animation, audio, UI, scene, and asset workflow;
- clean-machine packaged game workflow;
- production gameplay framework;
- finished product sample or tutorial game.

## Online And Persistent Worlds

Current evidence does not establish a complete:

- multiplayer gameplay product;
- replication, snapshot, prediction, reconciliation, and interpolation loop;
- persistent player and world-state product;
- reconnect and restart-recovery product flow;
- zone, shard, partition, or interest-management implementation at MMO scale;
- community-server administration product;
- production anti-cheat, abuse prevention, moderation, or security model;
- soak, load, concurrency, or scale certification;
- creator-driven persistent community-world product.

The selected Persistent Community Worlds direction is documented in
[SAGA_MMO_GENRE_FOCUS.md](SAGA_MMO_GENRE_FOCUS.md). It is a prioritization
decision, not proof of implementation.

## Editor And Authoring

Current evidence does not establish a complete:

- editor product workflow;
- scene, prefab, asset, component, and project authoring experience;
- Visual Blocks editing environment;
- high-level and low-level block UX;
- arbitrary or lossless C# to blocks round trip;
- source-preserving graph editing for all supported C#;
- collaboration, presence, locking, permissions, CRDT, or team-editing product.

C# is the intended canonical behavior source, but the complete runtime,
authoring, editor, hot-reload, debugging, and deployment path is not yet
established as a finished product.

## Creator And Trust Security

Current evidence does not establish:

- arbitrary untrusted C# or native code execution;
- a secure user-script sandbox;
- capability-based user-code permissions;
- resource budgets for untrusted scripts;
- package signing and trust-chain enforcement;
- complete third-party provenance, SBOM, and license-policy enforcement;
- automated tainted-source detection or complete clean-room verification;
- production moderation and abuse controls;
- safe public user-generated-code hosting.

Trusted developer and reviewed-package workflows must not be described as an
untrusted creator platform.

## Assets And Samples

Current evidence does not establish a complete:

- asset import, cook, cache, dependency, and migration pipeline;
- production shader, material, texture, mesh, animation, or audio pipeline;
- deterministic cross-platform cooked-asset compatibility guarantee;
- large-world or MMO-scale asset streaming system;
- finished sample game;
- finished playable `MultiplayerSandbox`.

Placeholder, fixture, staged, or report-only artifacts must retain their actual
classification.

## Rendering And Platform Coverage

Current evidence does not establish:

- production renderer readiness;
- complete Vulkan, Direct3D, OpenGL, or cross-vendor parity;
- complete device-loss and swap-chain recovery;
- production GPU synchronization and deferred destruction;
- fragmentation-resistant GPU memory allocation;
- complete render graph execution;
- asset-driven PBR, character, VFX, atmosphere, water, or terrain systems;
- cross-GPU, driver, backend, HDR, frame-pacing, or performance certification;
- complete Windows, Linux, macOS, mobile, web, console, XR, or platform release
  support.

Focused GPU tests prove only their declared configurations and behaviors.

## Evidence And Diagnostics

Current evidence does not establish:

- a permanent full-suite pass guarantee;
- complete stress, fuzz, sanitizer, soak, benchmark, or performance coverage;
- production telemetry, crash reporting, fault replay, or observability;
- complete architecture-boundary enforcement;
- that report-only checks automatically block invalid behavior;
- that historical milestone evidence is still current.

Every positive claim must name or link to discoverable evidence and preserve
its limitations.

## Release And SDK

Current evidence does not establish:

- product beta or release-candidate status;
- production readiness;
- enterprise readiness;
- a stable public SDK;
- long-term C++ binary ABI stability;
- complete semantic-versioning or compatibility guarantees;
- a finalized official-distribution or trademark policy;
- an installer, updater, launcher, signing service, marketplace, or publishing
  platform;
- hosted collaboration, managed operations, LTS, or enterprise service
  availability;
- supported production deployment or commercial support commitments.

Use [CURRENT_CAPABILITIES.md](CURRENT_CAPABILITIES.md) for the bounded current
capability summary.
