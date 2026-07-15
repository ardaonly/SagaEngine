---
title: What is SagaEngine?
status: current
owner: SagaWiki
generated_html: pages/product.html
---

# Product position

SagaEngine currently provides bounded engine, runtime, server-authority, editor, packaging, diagnostics, and project-tooling foundations.

## Engine surface and project depth

SagaEngine keeps a general-purpose engine and authoring surface while giving deeper architectural attention to creator-driven persistent community worlds. Engine and runtime layers provide reusable authority, replication, persistence, package, and world mechanisms; fixed gameplay, economy, progression, and social rules belong in project or package code. This direction prioritizes reusable boundaries and does not claim that a complete persistent-world product exists.

## Current capabilities

- C++ runtime modules for core services, ECS, simulation, rendering, input, audio, assets, resources, networking, replication, UI, scripting, persistence, and world systems.
- Editor modules for the core model, framework, Qt implementation, authoring, Visual Blocks, scripting, collaboration, and experiments.
- SagaProjectKit project-manifest validation and resolution, SagaScript C# analysis/compilation workflows, and SagaPackager package inspection and staging.
- Focused unit, architecture, integration, GPU, installed-consumer, stress-support, and evidence tests.

## Product roles

Saga is the launcher shell, SagaEditor is the editor role, and SagaRuntime is the runtime role. Their presence does not establish a polished end-to-end creation workflow or public SDK.

## Public-tool direction

The intentionally small public CLI surface is `sagaproject`, `sagascript`, and `sagapack`. Developer checks remain internal unless this wiki explicitly promotes them.
