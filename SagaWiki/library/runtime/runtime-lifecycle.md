---
title: Runtime lifecycle
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Runtime lifecycle

SagaRuntime is the runtime composition owner; individual modules own their domain contracts and implementation. Program entry points should configure and launch this composition rather than duplicate resource, simulation, rendering, or shutdown behavior.

## Startup sequence

A robust runtime startup is ordered and fail-fast:

1. resolve and validate the project manifest;
2. establish diagnostics before subsystems that can fail;
3. configure assets, packages, scripting, platform, and rendering through their owners;
4. create world and simulation state;
5. enter the frame or service loop only after required initialization succeeds.

Shutdown runs in the reverse ownership order. Native backend objects must not outlive the factory or device context that created them, and generated/package artifacts must not be assumed valid solely because a path exists.

## Frame contract

The normal rendering lifecycle is BeginFrame, submit work, and EndFrame. Global GPU idle waits are diagnostic or teardown tools, not the normal per-frame synchronization model. Frame ownership must be explicit enough that capture and failure paths cannot accidentally create a second device lifecycle.

## Content boundary

Runtime code consumes project-owned manifests, packages, assets, scenes, and scripts through their declared validation boundaries. Samples are useful evidence inputs, but the engine must not turn a sample path into hidden global configuration.

## Evidence limit

Runtime programs and smoke tests demonstrate the paths they execute. They do not by themselves establish a stable public SDK, all-platform support, or a complete launch workflow.

See [Runtime lifecycle, assets, and packages](../reference/runtime-lifecycle-assets-and-packages.md) for ordered composition, startup/shutdown, manifest layers, validation, reload, diagnostics, and review rules.
