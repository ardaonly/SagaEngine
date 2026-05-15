# SagaEngine — Tooling Ecosystem Roadmap

> Last updated: 2026-05-14  
> Target: Tool ownership, boundaries, and product relationship for SagaEngine’s external and internal tooling.

---

## 1. Purpose

This document is the index for SagaEngine’s tool ecosystem.

It does not replace individual tool roadmaps. Each production tool owns its own detailed roadmap inside its tool directory.

This document answers:

- which tools exist,
- what each tool is responsible for,
- whether the tool is product-facing or developer-facing,
- how the tool relates to Saga,
- which boundaries must not be crossed.

SagaEngine tooling must stay useful without turning into a second engine, second editor, or hidden product shell.

---

## 2. Tooling Principles

- Tools are not the engine.
- Tools are not the editor.
- Tools are not the Saga product shell.
- Tools may support Saga, but they must not silently own Saga product workflows.
- Standalone tools must remain extractable into standalone repositories.
- Tool-specific logic must live inside the owning tool.
- Shared product behavior belongs in Saga, not in tool dispatchers.
- Build orchestration belongs in Forge, not in SagaTools.
- Code intelligence belongs in Prism, not in Forge or Saga.
- Data compilation belongs in SDE, not in the editor or runtime.
- Tool dispatch belongs in SagaTools, not in each individual tool.

---

## 3. Tool Inventory

| Tool | Role | Product Relationship | Ownership |
|---|---|---|---|
| SDE | Deterministic data compiler | Used by Saga/editor/runtime for validation and compiled graph/artifact output | Standalone tool/compiler |
| Forge | Build workflow frontend over CMake/Conan/Nix-aware environments | Developer build interface | Standalone build tool |
| Prism | Codebase indexer and AI memory graph builder | Developer intelligence and repository analysis | Standalone analysis tool |
| SagaTools | Thin tool dispatcher | Convenience entry point for registered tools | Standalone dispatcher |

---

## 4. SDE — System Definition Engine

SDE is a standalone, engine-independent deterministic data compiler.

It owns:

- schema and data loading,
- validation,
- structured diagnostics,
- deterministic compiled graph output,
- artifact hashes,
- dependency manifests,
- immutable runtime graph contracts.

SDE does not own:

- editor UI,
- product shell,
- runtime mutation,
- server authority,
- collaboration sessions,
- project lifecycle.

Rules:

- SDE must not include SagaEngine, SagaEditor, SagaServer, Prism, Forge, or SagaTools headers.
- Saga/editor/runtime may consume SDE through explicit integration points.
- Editor mutation means source-data mutation followed by SDE validation/compile.
- Failed compile must not publish partial graphs or fake artifact state.

Detailed roadmap:

```txt
Tools/SystemDefinitionEngine/SDE_ROADMAP.md