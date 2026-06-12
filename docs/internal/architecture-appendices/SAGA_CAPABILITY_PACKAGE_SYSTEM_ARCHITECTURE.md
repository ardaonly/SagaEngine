# Saga Capability Package System Architecture

## Document Status

**Status:** Proposed appendix / future capability package system direction
**Current appendix location:** `docs/internal/architecture-appendices/SAGA_CAPABILITY_PACKAGE_SYSTEM_ARCHITECTURE.md`
**Related systems:** SagaEngine Core, Saga Package System, SagaEditor, SagaRuntime, SagaServer, SagaTools, SDE, SagaWeaver, Forge, Prism, Build/Publish Pipeline
**Primary goal:** Define the general package/extension architecture that allows Saga projects to opt into gameplay, MMO, editor, blocks, scripting, tooling, platform, and library capabilities without bloating SagaCore, breaking source-preserving authoring, or turning the editor/runtime into uncontrolled plugin chaos.

This document is a proposed direction appendix, not current implementation
truth or a product readiness claim. It does not claim that
SagaEngine currently implements a package resolver, package lockfile, dynamic
plugin loader, marketplace, production plugin security model, arbitrary package
hot reload, or public package SDK. Current product and architecture claims
remain governed by `docs/product/` and the current summaries in
`docs/architecture/`.

---

# 1. Executive Summary

Saga should not have a narrow **MMO genre plugin system**.

Saga should have a **Capability Package System**.

A capability package is a versioned, manifest-described unit that can add one or more of the following to a Saga project:

```text
runtime systems
server authority systems
asset schemas
editor authoring surfaces
visual block nodes
C# node libraries
SagaWeaver metadata inputs
SDE schemas and graph definitions
cook/build/package steps
validators
diagnostics views
native/library integrations
platform integrations
optional scripting/modding providers
project templates
```

This system exists so Saga can remain small at the core while still being highly customizable at the product, editor, project, runtime, and server layer.

Correct high-level model:

```text
SagaCore
  small MMO-first, genre-neutral substrate

SagaPackageSystem
  package discovery, resolution, dependency graph, lockfile, permissions, policy profiles, activation profiles

SDE
  compiles package schemas, graph definitions, validation rules, artifact schemas, and editor-surface declarations into deterministic artifacts

SagaWeaver
  connects C# source and visual blocks through source-preserving analysis, source maps, node metadata, patch previews, and runtime bindings

SagaEditor
  shows only enabled and visible package authoring surfaces, panels, blocks, asset editors, validators, diagnostics, and configuration views

SagaRuntime / SagaServer
  load only enabled runtime/server modules, compiled C# assemblies, runtime bindings, and package artifacts

SagaTools
  validate, cook, build, package, smoke-test, profile, migrate, and report using the same resolved package graph
```

The key rule:

```text
Packages add capabilities.
Core owns the stable substrate.
Editor surfaces are optional and user-configurable.
Runtime/server hot paths remain typed/compiled.
C# remains the official source-preserving scripting path.
Package identity, versioning, activation, migration, permissions, and diagnostics must be explicit.
```

---

# 2. Product Claim

Allowed claim:

```text
Saga provides a manifest-driven Capability Package System where projects can opt into runtime, server, editor, blocks, scripting, tooling, platform, and library capabilities through versioned manifests, deterministic SDE artifacts, SagaWeaver source-preserving metadata, package lockfiles, policy profiles, and binary-specific activation profiles.
```

Shorter product sentence:

```text
SagaCore stays small; Saga projects grow by enabling capability packages.
```

Forbidden claims:

```text
Saga has a complete marketplace.
Saga can safely run arbitrary plugins without permissions.
Every third-party package is trusted.
Every scripting language participates in C# <-> Blocks roundtrip.
SagaCore contains every MMO gameplay system.
SagaEditor hardcodes every MMO genre editor.
Blocks replace C# as the source of truth.
Runtime executes visual graphs as the main gameplay path.
Packages can be disabled without dependency analysis.
Package migration is automatic without explicit migration contracts.
Runtime hot reload is supported for arbitrary packages.
Dynamic plugins are ABI-safe without version checks.
```

---

# 3. Core Principles

## 3.1 Core Is Small, MMO-First, and Genre-Neutral

SagaCore should know about common MMO substrate concerns:

```text
authority
replication
persistence
simulation tick
interest management
zones / shards / instances
resource/package manifests
diagnostics
artifact references
package loading contracts
runtime/server module registration
editor extension contracts
script/block metadata consumption
```

SagaCore should not know about concrete genre or integration details:

```text
quest systems
class/level systems
raids
loot progression
hunger/thirst
crafting rules
weapon recoil
RTS unit commands
Lua syntax
Python syntax
Android API details
HTTP client implementation
Discord integration
database driver implementation
```

Those belong in capability packages.

---

## 3.2 Capability Packages Are Not Just Plugins

`Plugin` is the physical extension mechanism.

`Capability package` is the architectural unit.

A capability package may contain:

```text
schemas
definitions
code modules
editor surfaces
block definitions
script metadata
validators
diagnostics
native dependencies
platform adapters
templates
documentation
migration rules
policy declarations
test fixtures
```

Therefore, this system should be called:

```text
Saga Capability Package System
```

not merely:

```text
Plugin System
```

---

## 3.3 Installed Does Not Mean Enabled

A package can exist on disk without affecting a project.

Required states:

```text
Installed
  Package exists in an engine, project, user cache, or registry location.

Resolved
  Package was selected by the project manifest and lockfile resolution.

Enabled
  Package is active for a project profile.

Activated
  A specific binary loaded the relevant module/artifacts.

Visible
  The editor shows a specific authoring surface/panel in the current workspace.
```

These are separate states.

Important rule:

```text
A package directory on disk must not automatically mutate project behavior.
```

The project manifest and lockfile are the source of truth for the active package graph.

---

## 3.4 Package Activation Is Binary-Specific

A single package can provide different pieces for different binaries.

Example:

```text
SagaEditor loads:
  editor module
  block descriptors
  asset schemas
  editor surfaces
  validators
  diagnostics views
  permission views

SagaRuntime loads:
  runtime module
  runtime assets
  compiled C# assembly
  runtime bindings

SagaServer loads:
  server module
  server authority policy
  persistence/replication rules

SagaCooker loads:
  cook module
  asset validators
  package emitters
  migration validators

SDE loads:
  .sde schema/graph/definition files

SagaWeaver loads:
  C# node libraries
  script compatibility metadata
  source map inputs

CI / PublishGate loads:
  manifests
  lockfiles
  reports
  policy profiles
```

Server should not load editor panels.

Runtime should not load editor graph canvas code.

SDE should not load runtime implementation.

---

## 3.5 Editor Customization Must Be Controlled

SagaEditor should be maximally customizable through controlled extension points.

Allowed editor extension points:

```text
PanelRegistry
InspectorRegistry
AssetTypeRegistry
BlockNodeRegistry
GraphEditorRegistry
ValidatorRegistry
DiagnosticsViewRegistry
CookStepRegistry
ProjectTemplateRegistry
ToolbarRegistry
MenuRegistry
PermissionViewRegistry
WorkspaceLayoutRegistry
PackageConfigRegistry
MigrationViewRegistry
ProfilerViewRegistry
```

Packages can add surfaces to these registries.

Packages should not be allowed to:

```text
override editor lifecycle arbitrarily
mutate project source-of-truth without validated transactions
bypass server authority policy
read private engine/editor state directly
silently modify other packages' schemas
inject untracked native dependencies
enable network/native/script execution without declared permissions
```

---

## 3.6 C# Remains the Official Source-Preserving Authoring Path

Saga's official script-backed authoring path is:

```text
C# source
  -> SagaWeaver analysis
  -> visual block projection
  -> minimal source patch when supported
  -> C# compile
  -> runtime_bindings.json
  -> runtime/server execution
```

Alternative languages such as Lua or Python may exist as **project/mod scripting providers**, not as replacements for the official C# + SagaWeaver authoring path.

Correct:

```text
C# + SagaWeaver = engine authoring path
Lua/Python = optional game-facing or player-facing mod scripting provider
```

Incorrect:

```text
Lua replaces Saga scripting.
Python participates in normal C# <-> Blocks roundtrip.
Visual graph becomes the runtime execution model.
```

---

## 3.7 Runtime Hot Path Must Remain Typed/Compiled

Editor extension can be dynamic.

Runtime/server hot path should not be dynamic by default.

Correct:

```text
package loads at startup
systems register with typed scheduler
C# compiles into assembly
runtime_bindings.json maps stable function ids
native systems run as typed modules
Lua/Python runs only through explicit sandbox/mod surfaces
```

Incorrect:

```text
every frame calls plugin->Call("Update", stringName)
every entity update uses reflection dispatch
every block is interpreted at runtime
every gameplay path crosses dynamic string APIs
```

---

## 3.8 Migration, Compatibility, and Disable Safety Are First-Class

A package may define data schemas, network schemas, save data, editor metadata, block IDs, asset types, runtime bindings, and server policies.

Therefore, a package is not just code.

Any package that writes persistent data, contributes network protocol state, or defines asset formats must declare:

```text
schema version
migration policy
compatibility range
unsupported version behavior
default value policy
save/load behavior
network compatibility behavior
degraded mode behavior
```

---

# 4. System Overview

## 4.1 High-Level Architecture

```text
                 +-------------------------+
                 |      Game Project       |
                 | saga.project.json       |
                 | saga.packages.lock.json |
                 +-----------+-------------+
                             |
                             v
                 +-------------------------+
                 |  SagaPackageSystem      |
                 | resolve / lock / policy |
                 | permissions / profiles  |
                 +-----------+-------------+
                             |
        +--------------------+--------------------+
        |                    |                    |
        v                    v                    v
+---------------+    +----------------+   +----------------+
|      SDE      |    |   SagaWeaver   |   |  Module Loader |
| .sde -> IR    |    | C# -> Blocks   |   | runtime/server |
| artifacts     |    | source maps    |   | editor/tools   |
+------+--------+    +-------+--------+   +-------+--------+
       |                     |                    |
       v                     v                    v
+---------------+    +----------------+   +----------------+
| Schema/Graph  |    | node metadata  |   | loaded modules |
| Diagnostics   |    | runtime binds  |   | registered APIs|
| Source Maps   |    | projections    |   |                |
+------+--------+    +-------+--------+   +-------+--------+
       |                     |                    |
       +----------+----------+----------+---------+
                  |                     |
                  v                     v
          +---------------+     +----------------+
          |  SagaEditor   |     | Runtime/Server |
          | authoring UI  |     | execution      |
          +---------------+     +----------------+
```

---

## 4.2 Responsibility Split

| System | Owns | Must Not Own |
|---|---|---|
| `SagaCore` | MMO substrate, diagnostics primitives, package host contracts | Quest/FPS/RTS/scripting language implementations |
| `SagaPackageSystem` | package discovery, dependency resolution, lockfile, permissions, policy profiles, activation graph | Runtime execution, editor UI implementation, SDE compiler passes |
| `SDE` | deterministic definition compilation, schemas, graph IR, diagnostics, source maps, dependency manifests | SagaEditor UI, runtime/server execution, C# compiler, package manager behavior |
| `SagaWeaver` | source-preserving C# <-> Blocks analysis, node metadata, source maps, runtime bindings | Full IDE, arbitrary C# conversion, runtime visual graph VM, Lua/Python host |
| `SagaEditor` | authoring shell, panels, inspectors, block UI, graph UI, workspace layout | Package dependency solving, runtime authority, script source truth |
| `SagaRuntime` | client/runtime module execution and package consumption | Editor panels, authoring UI |
| `SagaServer` | authoritative server module execution | Editor UI, client-only runtime assumptions |
| `SagaTools` | validate/cook/build/package/smoke/profile/report/migration commands | Hidden project mutation without reports |
| `Forge` | build workflow invocation and orchestration | SDE internals, editor UI internals |
| `Prism` | artifact/code intelligence from public outputs | SDE parser/AST internals |

---

# 5. Package Kinds

A package may declare one or more kinds.

Recommended package kinds:

```text
CapabilityKit
  Adds gameplay/system capability.

AuthoringPack
  Adds editor-facing authoring surfaces.

BlockLibrary
  Adds visual block node definitions and/or C# node metadata.

ScriptingProvider
  Adds optional project/mod scripting support such as Lua or Python.

NativeLibraryAdapter
  Adds integration to a native or external library.

PlatformIntegration
  Adds platform-specific capability such as Android, console, mobile, cloud.

ToolingExtension
  Adds cook/build/validate/package/publish tooling steps.

DiagnosticsPack
  Adds validators, reports, diagnostics views, profiling data, or quality gates.

ProjectTemplate
  Adds a starter project/preset profile.

SchemaPackage
  Adds SDE schemas, graph models, validation declarations, and artifact schemas.

MigrationPack
  Adds explicit package/project/asset save-data migration logic.

PolicyPack
  Adds organization/project policy profiles.
```

---

# 6. Capability Composition Instead of Fixed MMO Genres

Saga should not hard-select exactly three MMO genres.

Instead, MMO genres should be built by composing capability packages.

## 6.1 Example: Avatar RPG MMO

```text
saga.gameplay.foundation
saga.inventory
saga.equipment
saga.quest
saga.dialogue
saga.guild
saga.party
saga.avatar
saga.loot
saga.dungeon
```

## 6.2 Example: Survival Sandbox MMO

```text
saga.gameplay.foundation
saga.inventory
saga.crafting
saga.building
saga.territory
saga.resource-nodes
saga.world-persistence
saga.decay
```

## 6.3 Example: Action / MMOFPS

```text
saga.gameplay.foundation
saga.weapon
saga.loadout
saga.team-objective
saga.hit-validation
saga.lag-compensation
saga.respawn
```

## 6.4 Example: MMORTS

```text
saga.gameplay.foundation
saga.rts-command
saga.unit-selection
saga.fog-of-war
saga.formation
saga.economy
saga.mass-simulation
```

This avoids turning SagaCore into a mixed MMORPG/FPS/RTS/survival mega-core.

---

# 7. Repository and File Architecture

## 7.1 Engine-Level Package Locations

Recommended repository layout:

```text
SagaEngine/
  Engine/
    Public/
    Private/

  Tools/
    SystemDefinitionEngine/
    SagaWeaver/
    SagaPackager/
    SagaProject/
    SagaScript/

  Packages/
    Official/
      Saga.GameplayFoundation/
      Saga.Inventory/
      Saga.Quest/
      Saga.Lua/
      Saga.Android/

    Experimental/
      Saga.RTSCommand/
      Saga.Python/

  Samples/
    StarterArena/
    MultiplayerSandbox/

  docs/
    architecture/
    product/
```

## 7.2 Project-Level Package Locations

```text
MyGame/
  saga.project.json
  saga.packages.lock.json

  Assets/
  Scripts/
  Config/

  Packages/
    Company.CustomCombat/
    Company.CustomEconomy/
    Company.PrivateLuaApi/

  Build/
    SDE/
    SagaWeaver/
    Reports/
    Packages/
```

## 7.3 User Cache / Marketplace Location

```text
~/.saga/
  packages/
    cache/
      downloaded-package-id/
    registries/
      official.registry.json
      company.registry.json
```

The user cache is a resolution source, not a source-of-truth.

The lockfile must still determine the exact package identity and version.

---

# 8. Standard Package Layout

A package should use a stable directory shape.

```text
Packages/Official/Saga.Quest/
  package.saga.json

  Schemas/
    quest.schema.sde
    quest_graph.schema.sde

  Blocks/
    quest_blocks.sde

  Weaver/
    quest_weaver.sde
    NodeLibraries/
      SagaQuest.Nodes.csproj

  Runtime/
    include/
    src/
    SagaQuest.Runtime.module.json

  Server/
    include/
    src/
    SagaQuest.Server.module.json

  Editor/
    quest_authoring.sde
    src/
    SagaQuest.Editor.module.json

  Cook/
    SagaQuest.Cook.module.json

  Diagnostics/
    quest_diagnostics.sde

  Migration/
    quest_migrations.sde
    src/

  Policy/
    quest_policy.sde

  Templates/
    AvatarRPGQuestStarter/

  docs/
    README.md
    KNOWN_LIMITATIONS.md

  tests/
    fixtures/
    golden/
```

Not every package must include every directory.

Minimal package:

```text
Saga.Inventory/
  package.saga.json
  Schemas/
  Runtime/
  Diagnostics/
```

Editor-only package:

```text
Company.CustomEditorTools/
  package.saga.json
  Editor/
  Diagnostics/
```

Scripting provider package:

```text
Saga.Lua/
  package.saga.json
  Schemas/
  Runtime/
  Server/
  Editor/
  Cook/
  Diagnostics/
```

Migration-only package:

```text
Company.LegacyQuestMigration/
  package.saga.json
  Migration/
  Diagnostics/
```

---

# 9. Package Manifest

## 9.1 Required Fields

```json
{
  "schemaVersion": 1,
  "id": "saga.quest",
  "kind": ["CapabilityKit", "AuthoringPack", "BlockLibrary"],
  "version": "0.1.0",
  "maturity": "technical-preview"
}
```

## 9.2 Full Example

```json
{
  "schemaVersion": 1,
  "id": "saga.quest",
  "displayName": "Saga Quest Kit",
  "kind": ["CapabilityKit", "AuthoringPack", "BlockLibrary", "DiagnosticsPack"],
  "version": "0.1.0",
  "maturity": "technical-preview",

  "compatibility": {
    "sagaEngine": ">=0.1.0 <0.2.0",
    "packageAbiVersion": 1,
    "saveSchemaVersion": 3,
    "networkSchemaVersion": 2,
    "editorSchemaVersion": 1,
    "artifactSchemaVersion": 1
  },

  "provides": [
    "asset.quest",
    "graph.quest",
    "block.quest.start",
    "block.quest.complete",
    "validator.quest-graph",
    "runtime.quest-state",
    "server.quest-authority",
    "editor.quest-graph"
  ],

  "requires": [
    { "id": "saga.core", "version": ">=0.1.0" },
    { "id": "saga.gameplay.foundation", "version": ">=0.1.0" }
  ],

  "conflictsWith": [
    "company.legacy-quest"
  ],

  "replaces": [],

  "modules": {
    "runtime": {
      "path": "Runtime/SagaQuest.Runtime",
      "loadMode": "dynamic-or-static",
      "abiVersion": 1
    },
    "server": {
      "path": "Server/SagaQuest.Server",
      "loadMode": "dynamic-or-static",
      "abiVersion": 1
    },
    "editor": {
      "path": "Editor/SagaQuest.Editor",
      "loadMode": "editor-only",
      "abiVersion": 1
    },
    "cook": {
      "path": "Cook/SagaQuest.Cook",
      "loadMode": "tool-only",
      "abiVersion": 1
    }
  },

  "sde": {
    "schemas": [
      "Schemas/quest.schema.sde",
      "Schemas/quest_graph.schema.sde"
    ],
    "blocks": [
      "Blocks/quest_blocks.sde"
    ],
    "editorSurfaces": [
      "Editor/quest_authoring.sde"
    ],
    "diagnostics": [
      "Diagnostics/quest_diagnostics.sde"
    ],
    "migration": [
      "Migration/quest_migrations.sde"
    ],
    "policy": [
      "Policy/quest_policy.sde"
    ]
  },

  "weaver": {
    "nodeLibraries": [
      "Weaver/NodeLibraries/SagaQuest.Nodes.csproj"
    ],
    "sourcePreserving": true
  },

  "permissions": [
    "project.asset-read",
    "project.asset-write"
  ],

  "editorSurfaces": [
    {
      "id": "saga.quest.editor.graph",
      "displayName": "Quest Graph",
      "defaultVisible": false,
      "canDisable": true
    },
    {
      "id": "saga.quest.inspector.quest",
      "displayName": "Quest Inspector",
      "defaultVisible": true,
      "canDisable": true
    }
  ],

  "migration": {
    "strategy": "explicit-migration-required",
    "supportsRollback": true,
    "fromVersions": ["0.1.0", "0.1.1"],
    "toVersion": "0.2.0"
  },

  "observability": {
    "requiredCounters": [
      "package.load.ms",
      "system.tick.ms",
      "server.authority.rejections",
      "persistence.transactions",
      "replication.bytes"
    ]
  },

  "nonClaims": [
    "not a complete MMORPG",
    "not production-ready",
    "does not replace Saga C# scripting"
  ]
}
```

---

# 10. Project Manifest

The project manifest declares requested packages.

```json
{
  "schemaVersion": 1,
  "project": {
    "name": "MyMMO",
    "sagaVersion": "0.1.0"
  },

  "packages": [
    { "id": "saga.gameplay.foundation", "version": "0.1.0" },
    { "id": "saga.inventory", "version": "0.1.0" },
    { "id": "saga.quest", "version": "0.1.0" },
    { "id": "saga.lua", "version": "0.1.0" },
    { "id": "company.http-client", "version": "1.2.0" }
  ],

  "profiles": {
    "editor": "default-authoring",
    "runtime": "client",
    "server": "authoritative",
    "cook": "strict",
    "policy": "development"
  }
}
```

---

# 11. Package Lockfile

The lockfile records exact resolved packages.

```json
{
  "schemaVersion": 1,
  "sagaVersion": "0.1.0",
  "packages": [
    {
      "id": "saga.quest",
      "version": "0.1.0",
      "source": "engine-official",
      "contentHash": "sha256:...",
      "manifestHash": "sha256:...",
      "packageAbiVersion": 1,
      "saveSchemaVersion": 3,
      "networkSchemaVersion": 2
    },
    {
      "id": "company.http-client",
      "version": "1.2.0",
      "source": "project-local",
      "contentHash": "sha256:...",
      "manifestHash": "sha256:...",
      "packageAbiVersion": 1
    }
  ]
}
```

Rule:

```text
Build/cook/package must use the lockfile, not a floating package search.
```

---

# 12. Package Resolution

## 12.1 Resolution Inputs

```text
saga.project.json
saga.packages.lock.json
engine package registries
project-local package directories
user cache / marketplace cache
development override paths
target binary/profile
policy profile
```

## 12.2 Resolution Order

Recommended:

```text
1. Project manifest declares requested package ids and versions.
2. Lockfile pins exact resolved identity and content hashes.
3. Resolver checks project-local packages.
4. Resolver checks engine official/experimental packages.
5. Resolver checks user cache / registry.
6. Resolver checks development override paths only when explicitly enabled.
7. Resolver validates dependency graph, capability conflicts, policies, and permissions.
8. Resolver emits package graph report.
```

## 12.3 Validation Rules

Resolver must detect:

```text
missing package
unsupported version
duplicate package id
duplicate capability provider
ambiguous capability provider
dependency cycle
permission mismatch
policy violation
binary-incompatible module
ABI mismatch
missing required artifact
stale lockfile
manifest hash mismatch
content hash mismatch
disabled package still referenced by assets
unsupported save schema
unsupported network schema
```

---

# 13. Capability Conflict and Override Policy

Two packages may attempt to provide the same capability.

Examples:

```text
saga.lua and company.custom-lua both provide script-language.lua
saga.inventory and company.inventory both provide asset.inventory
two packages register the same block id
two packages register incompatible authority policy
```

The resolver must detect:

```text
duplicate capability provider
ambiguous provider
incompatible provider
unsupported override
replaced package without explicit approval
```

Recommended manifest fields:

```json
{
  "conflictsWith": [
    "company.legacy-inventory"
  ],
  "replaces": [
    "saga.inventory"
  ],
  "overridePolicy": "explicit-project-opt-in"
}
```

Required rule:

```text
A package cannot silently replace another package's capability.
Project manifest must explicitly approve replacements.
```

---

# 14. Package Versioning, ABI, Save Migration, and Network Compatibility

## 14.1 Compatibility Dimensions

A package may have multiple compatibility dimensions:

```text
Saga engine compatibility
package ABI compatibility
runtime module ABI compatibility
editor module ABI compatibility
tool module ABI compatibility
save schema compatibility
network schema compatibility
asset schema compatibility
block schema compatibility
SagaWeaver metadata compatibility
SDE artifact schema compatibility
```

These must not be collapsed into one generic version string.

## 14.2 Required Compatibility Fields

Recommended package fields:

```json
{
  "compatibility": {
    "sagaEngine": ">=0.1.0 <0.2.0",
    "packageAbiVersion": 1,
    "saveSchemaVersion": 3,
    "networkSchemaVersion": 2,
    "editorSchemaVersion": 1,
    "artifactSchemaVersion": 1
  }
}
```

## 14.3 Migration Rules

A package that writes persistent data must define:

```text
schema version
migration path
rollback support
missing field policy
unsupported version policy
default value policy
validation after migration
diagnostic report path
```

Required migration statuses:

```text
NotNeeded
Required
Previewed
Applied
RolledBack
Blocked
Failed
UnsupportedSourceVersion
UnsupportedTargetVersion
```

## 14.4 Network Compatibility Rules

A package that contributes replicated state, commands, or protocol messages must define:

```text
network schema version
client/server compatibility range
unsupported client behavior
unsupported server behavior
protocol negotiation metadata
replay compatibility behavior
```

Forbidden:

```text
silently accepting unknown replicated fields
silently ignoring authority-sensitive command fields
loading client/server package versions without compatibility check
```

---

# 15. Build Target and Link Direction Rules

The package architecture must be reflected in build targets.

Core targets must not link package targets.

Correct direction:

```text
Package target -> stable Engine public contracts
Package editor target -> stable Editor extension contracts
Package server target -> stable Server extension contracts
Package tool target -> stable Tool extension contracts
```

Incorrect direction:

```text
SagaCore -> SagaQuest
SagaRuntime -> SagaAvatarRPG
SagaEditor private implementation -> package private headers
SDE -> SagaEngine
```

Example correct target direction:

```cmake
target_link_libraries(SagaQuestRuntime
  PUBLIC
    SagaEngineRuntimeContracts
    SagaEngineDiagnostics
    SagaEnginePackageContracts
)
```

Forbidden:

```cmake
target_link_libraries(SagaEngineRuntime
  PRIVATE
    SagaQuestRuntime
)
```

## 15.1 Include Boundary Rules

Packages may include:

```text
Engine/Public
Editor/Public extension contracts
Server/Public extension contracts
Tools/Public extension contracts
PackageSystem/Public contracts
```

Packages may not include:

```text
Engine/Private
Editor/Private
Server/Private
SDE private compiler internals
SagaWeaver private analyzer internals
other package private headers unless explicitly exported
```

Required boundary tests:

```text
package targets do not include Engine/Private
core targets do not include package headers
SDE does not include Saga headers
SagaWeaver package bridge uses public metadata contracts
editor package targets do not include editor private implementation headers
```

---

# 16. Module ABI and Static/Dynamic Loading Policy

## 16.1 Load Modes

Allowed load modes:

```text
static
dynamic
dynamic-or-static
editor-only
tool-only
server-only
runtime-only
```

## 16.2 Development vs Shipping

Recommended policy:

```text
development/editor builds:
  dynamic loading allowed
  development override paths optionally allowed
  unsigned project-local packages may be allowed by policy

CI builds:
  lockfile required
  deterministic package reports required
  unsigned packages policy-controlled

shipping/client builds:
  lockfile required
  no development override paths
  editor modules excluded
  dynamic loading only if policy allows

dedicated server builds:
  package set fixed at startup
  editor modules excluded
  server-safe scripting only
  ABI/version checks mandatory
```

## 16.3 ABI Rules

Dynamic module loading requires:

```text
package ABI version check
module ABI version check
engine compatibility check
target platform compatibility check
permission check
diagnostic report on failure
```

Native module load failure must be deterministic and diagnosable.

---

# 17. Activation Profiles

Activation profile determines which package parts are active for which binary.

```json
{
  "profiles": {
    "editor": {
      "enableModules": ["editor", "diagnostics"],
      "enableArtifacts": ["schemas", "blocks", "editorSurfaces"],
      "enableWeaver": true
    },
    "runtime": {
      "enableModules": ["runtime"],
      "enableArtifacts": ["runtimeAssets", "runtimeBindings"]
    },
    "server": {
      "enableModules": ["server"],
      "enableArtifacts": ["authorityRules", "persistenceRules", "runtimeBindings"]
    },
    "cook": {
      "enableModules": ["cook"],
      "enableArtifacts": ["schemas", "validators", "cookRules"]
    }
  }
}
```

---

# 18. Package Policy Profiles

Permissions alone are not enough.

Projects need policy profiles.

Recommended profiles:

```text
development
  unsigned project packages allowed
  development override paths allowed
  dynamic editor modules allowed
  verbose diagnostics

ci
  lockfile required
  deterministic reports required
  unsigned packages rejected unless explicitly whitelisted
  development override paths rejected

shipping-client
  lockfile required
  editor modules rejected
  development override paths rejected
  permissions restricted
  native loading restricted by platform policy

dedicated-server
  server-safe packages only
  editor modules rejected
  server-safe scripting only
  fixed startup package graph
  network/schema compatibility required

enterprise
  signed packages required
  policy-reviewed native modules
  source visibility rules enforced
  package audit report required
```

Policy validation should emit:

```text
package_policy_report.json
```

---

# 19. Package Lifecycle and Hot Reload Boundary

## 19.1 Lifecycle States

```text
Discovered
Resolved
Validated
Loaded
Registered
Activated
Deactivated
Unloaded
Blocked
Failed
```

These states are not the same as editor visibility.

## 19.2 Hot Reload Boundary

Initial policy:

```text
No arbitrary runtime hot unload.
Dedicated server package set is fixed at startup.
Runtime package graph is fixed for a running session.
Limited editor-side reload may exist for authoring surfaces.
SDE/SagaWeaver artifacts can be regenerated during authoring.
```

Hot reload is allowed only when:

```text
package declares reload-safe behavior
no active runtime/server state depends on old version
asset/schema migration is not required
module ABI is compatible
editor transaction can be rolled back
diagnostics report confirms safe reload
```

Forbidden v0 claim:

```text
Saga supports runtime hot reload for arbitrary packages.
```

---

# 20. Package Disable, Degraded Mode, and Asset Dependency Analysis

Disabling a package requires dependency analysis.

Inputs:

```text
package graph
asset dependency graph
SDE dependency manifests
SagaWeaver source maps/runtime bindings
runtime/server package artifacts
project scripts
package-provided asset schemas
```

Before disabling a package, Saga must detect:

```text
assets using package asset types
scripts using package block nodes
C# source using package node libraries
runtime bindings referencing package functions
saved worlds using package save schema
server configs using package authority rules
cook outputs requiring package validators
```

Example diagnostic:

```text
Cannot disable saga.quest.

Dependent assets:
  Assets/Quests/intro.quest

Dependent scripts:
  Scripts/NpcIntro.cs uses block.quest.start

Dependent bindings:
  Build/SagaWeaver/runtime_bindings.json references SagaQuest.Nodes

Suggested actions:
  migrate assets
  remove references
  keep package enabled
  switch to degraded mode if package supports it
```

## 20.1 Degraded Mode

Some packages may support degraded mode.

Example:

```text
Quest authoring surface disabled:
  game still builds
  generic asset inspector remains available

Quest runtime disabled:
  quest assets become inactive
  project may still load if package declares degraded mode

Quest package removed:
  requires migration or asset removal
```

Degraded mode must be declared explicitly.

---

# 21. Editor Customization Model

## 21.1 Three Levels of Editor Control

SagaEditor should distinguish:

```text
Package Capability
  The project uses the package.

Authoring Surface Availability
  The package provides editor surfaces.

Workspace Visibility
  The user chooses which surfaces are visible in the current workspace.
```

Example:

```text
saga.quest package enabled
Quest runtime/server/schema active
Quest Editor surface available
Quest Editor panel hidden by user
```

The project still builds.

Only the UI is hidden.

---

## 21.2 Disabling Editor Surface vs Disabling Package

Panel hidden:

```text
Only workspace visibility changes.
No project dependency changes.
Safe.
```

Authoring pack disabled:

```text
Special editor surfaces are not shown.
Generic inspectors may still display assets.
Runtime/server systems may remain enabled.
Usually safe.
```

Package disabled:

```text
Capability is removed from the project.
Must run dependency analysis.
May require asset migration or deletion.
```

---

## 21.3 Workspace Layout Example

```json
{
  "schemaVersion": 1,
  "workspace": {
    "visibleSurfaces": [
      "scene.viewport",
      "asset.browser",
      "diagnostics",
      "saga.quest.inspector.quest"
    ],
    "hiddenSurfaces": [
      "saga.quest.editor.graph",
      "saga.dialogue.editor.graph"
    ]
  }
}
```

---

## 21.4 Schema-Driven Editor First

Every package should prefer schema-driven editor surfaces before custom editor code.

Recommended ratio:

```text
80% schema-driven editor
20% custom editor extension
```

SagaEditor should provide generic primitives:

```text
Generic Graph Editor
Generic Table Editor
Generic Inspector
Generic Asset Editor
Generic Diagnostics View
Generic Package Config View
Generic Permission View
Generic Migration View
Generic Profiler View
```

A package should provide:

```text
asset schema
graph schema
field metadata
validation rules
display metadata
optional custom surface
```

This prevents every MMO capability from requiring a full custom editor.

---

# 22. SDE Integration

SDE is a standalone deterministic compiler.

It should compile structured system/data definitions into deterministic artifacts.

In this architecture, SDE may compile package-owned definitions such as:

```text
asset schemas
block descriptors
graph schemas
validation rules
editor-surface declarations
diagnostic schemas
artifact schemas
dependency manifests
migration declarations
policy declarations
```

SDE must not own:

```text
SagaEditor UI
runtime execution
server authority implementation
C# compiler/scripting host
Lua/Python host
package resolver behavior
asset cooker implementation
package migration execution
```

Correct flow:

```text
Package .sde files
  -> SDE validate/compile
  -> canonical IR
  -> graph artifacts
  -> source maps
  -> dependency manifests
  -> diagnostics
  -> consumed by SagaEditor/Runtime/Server/Tools
```

SDE must remain reusable outside Saga.

Saga-specific behavior belongs in:

```text
schema packages
manifests
adapters
external consumers
post-SDE policy validators
```

---

# 23. SagaWeaver Integration

SagaWeaver owns the C# <-> Visual Blocks source-preserving authoring path.

It consumes:

```text
C# source files
C# node libraries from packages
Saga-compatible API metadata
package-provided node descriptors
SDE-emitted block/schema artifacts where applicable
```

It emits:

```text
analysis_report.json
compatibility_report.json
projection_report.json
source_map.json
node_metadata.json
runtime_bindings.json
patch_preview.json
sagascript_diagnostics.json
```

Important invariant:

```text
C# source remains canonical.
No-edit block projection must not mutate source.
Supported block edits patch minimal source spans.
Unsupported C# remains valid as opaque/read-only blocks.
Runtime executes compiled C#, not visual graphs.
```

## 23.1 Package-Provided C# Nodes

Example:

```csharp
[SagaLibrary("Quest")]
public static class QuestNodes
{
    [SagaNode("Start Quest")]
    public static void StartQuest(EntityId player, QuestId quest) { }
}
```

SagaWeaver extracts node metadata:

```text
QuestNodes.cs
  -> node_metadata.json
  -> block registry
  -> runtime_bindings.json
```

## 23.2 Package-Provided Block Descriptors

A package may also define block descriptors through SDE:

```text
block StartQuest {
  category: "Quest"
  inputs:
    player: EntityId
    quest: QuestId
  runtimeBinding: "SagaQuest.Nodes.StartQuest"
}
```

The bridge rule:

```text
SDE validates the descriptor.
SagaWeaver validates the C# binding/source-preserving path.
SagaEditor displays the block.
Runtime uses compiled binding metadata.
```

---

# 24. Lua / Python / Mod Scripting Providers

Alternative scripting languages are supported as packages, not as replacements for the official C# authoring path.

## 24.1 Correct Model

```text
C# + SagaWeaver
  official engine authoring path

Lua/Python Provider
  optional game-facing or player-facing mod scripting capability
```

## 24.2 Lua Provider Example

```text
Saga.Lua/
  package.saga.json

  Schemas/
    lua_script_asset.sde
    lua_mod_manifest.sde

  Runtime/
    Lua VM / sandbox / allowed API bindings

  Server/
    server-safe Lua execution policy

  Editor/
    Lua script asset inspector
    Lua mod manifest editor

  Cook/
    Lua script packaging
    syntax validation

  Diagnostics/
    lua_diagnostics.sde
```

## 24.3 Lua Package Manifest Example

```json
{
  "schemaVersion": 1,
  "id": "saga.lua",
  "kind": ["ScriptingProvider", "AuthoringPack", "DiagnosticsPack"],
  "version": "0.1.0",

  "provides": [
    "script-language.lua",
    "asset.lua-script",
    "editor.lua-inspector",
    "validator.lua-syntax",
    "runtime.lua-sandbox"
  ],

  "permissions": [
    "script.execute",
    "filesystem.project-read"
  ],

  "nonClaims": [
    "does not replace Saga C# scripting",
    "does not participate in C# source-preserving roundtrip",
    "not enabled unless the project opts in"
  ]
}
```

## 24.4 Blocks Calling Lua

A block may call a Lua mod function through C# provider API:

```text
Block:
  Run Lua Function
    mod: "weather"
    function: "OnRainStart"
```

C# representation:

```csharp
LuaMods.Call("weather", "OnRainStart");
```

SagaWeaver sees this as C# source projection.

Lua itself does not become part of the C# source-preserving roundtrip.

---

# 25. Native Library and Platform Integrations

Packages may integrate external libraries or platforms.

Examples:

```text
saga.http-client
saga.websocket
saga.android
saga.mobile-notifications
saga.postgres
saga.redis
saga.discord
company.analytics
company.cloud-save
```

These packages must declare:

```text
native dependencies
permissions
target platforms
binary compatibility
static/dynamic load mode
sandbox requirements
network/file/system access
```

Example permissions:

```json
{
  "permissions": [
    "network.client",
    "filesystem.project-read",
    "filesystem.project-write",
    "native-library.load",
    "server.runtime",
    "script.execute"
  ]
}
```

Editor must show permission requests before enabling risky packages.

---

# 26. Runtime and Service Topology

Saga must separate game runtime/server concerns from collaboration/editor/service concerns.

Different systems:

```text
SagaRuntime
  client/runtime execution

SagaDedicatedServer
  authoritative game simulation and replication

SagaGatewayServer
  connection routing, login/session entry, region routing where applicable

SagaPersistenceService
  database-backed state persistence service or adapter layer

SagaAuthService
  account/session/authentication service integration

SagaCollaborationServer
  editor/team collaboration, not game networking

SagaPackageRegistry
  package discovery/registry service, not gameplay runtime

SagaBuildService
  remote build/cook/package worker where applicable
```

Important distinctions:

```text
game server networking != editor collaboration networking
game persistence != editor workspace persistence
player auth != editor team auth
package registry != runtime package loader
```

Packages must declare where they are allowed to run:

```text
client-runtime
dedicated-server
gateway-service
persistence-service
editor
collaboration-server
cook-tool
ci-tool
```

---

# 27. Headless Usage

The package system must work without SagaEditor.

Required headless commands:

```bash
saga package resolve --project MyGame/saga.project.json
sde validate --project MyGame/saga.project.json
sde compile --project MyGame/saga.project.json
sagascript analyze --project MyGame/saga.project.json
saga cook --project MyGame/saga.project.json
saga server --project MyGame/saga.project.json
saga runtime --project MyGame/saga.project.json
saga package migrate --project MyGame/saga.project.json
```

Headless flow:

```text
project manifest
  -> package graph
  -> policy profile
  -> SDE artifacts
  -> SagaWeaver artifacts
  -> package/cook outputs
  -> runtime/server execution
```

No editor UI should be required.

---

# 28. Observability and Per-Package Performance Attribution

Every runtime/server/tool package should expose observable cost.

Required metrics where applicable:

```text
package load time
module activation time
system tick duration
server tick contribution
replication bandwidth contribution
dirty component count
interest management cost
persistence transaction duration
authority rejection count
script/mod execution cost
asset cook duration
validator duration
memory budget usage
native library load time
```

Reports:

```text
package_performance_report.json
server_package_tick_report.json
runtime_package_frame_report.json
cook_package_timing_report.json
script_provider_cost_report.json
```

Profiler rule:

```text
If a package makes the game/editor/server slow, Saga should be able to attribute the cost to the package, system, validator, script provider, or module.
```

---

# 29. Diagnostics and Evidence

Every package-related step should produce machine-readable evidence.

Required reports:

```text
package_resolution_report.json
package_permission_report.json
package_policy_report.json
package_activation_report.json
package_conflict_report.json
package_migration_report.json
package_disable_report.json
sde_compile_report.json
sagaweaver_script_report.json
editor_surface_report.json
runtime_module_report.json
server_module_report.json
cook_package_report.json
package_performance_report.json
```

Important statuses:

```text
Installed
Resolved
Enabled
Activated
Visible
Disabled
Blocked
Missing
Stale
PermissionDenied
PolicyDenied
VersionMismatch
AbiMismatch
DependencyCycle
CapabilityConflict
UnsafeDisable
MigrationRequired
MigrationFailed
UnsupportedSaveSchema
UnsupportedNetworkSchema
```

Saga should not say a package is "working" without:

```text
manifest validated
dependency graph resolved
capability conflicts resolved
permissions evaluated
policy profile evaluated
SDE artifacts generated when required
SagaWeaver artifacts generated when required
runtime/server/editor/tool activation tested for the relevant profile
migration status checked when applicable
diagnostics emitted
```

---

# 30. Security and Trust

Package trust levels:

```text
Official
Experimental
ProjectLocal
ThirdParty
Marketplace
Unsigned
DevelopmentOverride
```

Risky capabilities must require explicit permissions:

```text
network access
native library loading
file system write
server runtime execution
script execution
external process launch
editor source mutation
migration execution
package replacement
```

Packages should be rejected or sandboxed when:

```text
manifest missing
hash mismatch
unsigned where signature required
requests forbidden permission
targets unsupported platform
contains native module but project policy forbids native modules
tries to enable editor source mutation without validated transaction path
tries to replace another capability without explicit override approval
```

---

# 31. Testing Strategy

Required test families:

```text
manifest parsing tests
manifest validation tests
package dependency resolution tests
lockfile determinism tests
capability conflict tests
permission/policy profile tests
binary activation profile tests
static/dynamic ABI validation tests
editor surface visibility tests
authoring pack disable tests
full package disable dependency tests
asset dependency graph tests
SDE package bridge tests
SagaWeaver package node bridge tests
runtime module activation tests
server module activation tests
save/load golden snapshot tests
migration preview/apply/rollback tests
network schema compatibility tests
authority rejection tests
interest management visibility tests
deterministic replay tests
performance budget tests
headless workflow tests
public/private include boundary tests
target link direction tests
```

Closure rule:

```text
A package architecture increment does not close because the editor can show a panel.
It closes only when package resolution, activation, artifacts, diagnostics, policy, and relevant binary behavior are tested.
```

---

# 32. Non-Goals

This architecture does not require v0 to provide:

```text
public marketplace
hot plugin reload
untrusted third-party code sandbox
complete mod distribution platform
perfect package ABI stability
full visual scripting
Lua/Python block roundtrip
runtime graph interpreter
production plugin security model
all platform integrations
runtime hot unload
automatic migration without explicit package migration contracts
```

These may be future systems.

They should not be implied by the initial architecture.

---

# 33. Failure Modes

## 34.1 Core Bloat

Failure:

```text
SagaCore starts absorbing Quest/FPS/RTS/Lua/Android implementation details.
```

Mitigation:

```text
move domain systems into capability packages
keep core contract-focused
add boundary tests
```

## 34.2 Editor Surface Explosion

Failure:

```text
Every enabled package forces panels into the editor.
```

Mitigation:

```text
separate package enabled from surface visible
workspace layout controls visibility
generic editor surfaces first
```

## 34.3 Runtime Dynamic Dispatch Trap

Failure:

```text
Every gameplay path becomes string/reflection/plugin dispatch.
```

Mitigation:

```text
typed systems
compiled C#
runtime bindings
startup registration
no per-frame reflection dependency
```

## 34.4 SDE Coupling

Failure:

```text
SDE becomes a SagaEditor/package manager/runtime subsystem.
```

Mitigation:

```text
SDE emits artifacts only
Saga consumes artifacts externally
standalone SDE tests
no Saga module dependencies
```

## 34.5 SagaWeaver Scope Creep

Failure:

```text
SagaWeaver tries to become full IDE, arbitrary C# converter, Lua/Python bridge, or visual VM.
```

Mitigation:

```text
C# source-preserving scope
opaque unsupported C#
minimal source patching
runtime compiled C# path
```

## 34.6 Unsafe Package Execution

Failure:

```text
Package silently loads native/network/script code without user/project policy.
```

Mitigation:

```text
permissions
trust levels
lockfile
hash validation
activation diagnostics
policy gate
```

## 34.7 Migration Damage

Failure:

```text
Package migration rewrites assets/world data without preview, rollback, or diagnostics.
```

Mitigation:

```text
migration preview
explicit apply
backup/rollback where supported
golden snapshot tests
package_migration_report.json
```

## 34.8 ABI Instability

Failure:

```text
Dynamic package module loads against incompatible engine/module ABI.
```

Mitigation:

```text
ABI version fields
module compatibility validation
static/monolithic shipping option
diagnostic load failure
```

---

# 35. Final Decision

Saga should implement a general **Capability Package System**, not a narrow MMO genre plugin model.

The architecture should preserve this split:

```text
SagaCore:
  small MMO-first substrate

PackageSystem:
  package graph, permissions, policy profiles, lockfile, activation, conflicts, migration state

SDE:
  deterministic definition compiler for schemas, graphs, editor-surface declarations, diagnostics, source maps, dependency manifests, migration declarations, policy declarations

SagaWeaver:
  C# source-preserving blocks bridge, node metadata, source maps, patch previews, runtime bindings

SagaEditor:
  configurable authoring shell that renders only enabled and visible package surfaces

SagaRuntime/SagaServer:
  binary-specific execution of enabled runtime/server modules

SagaTools:
  headless validation, cook, build, migrate, package, smoke, profile, reports

Capability Packages:
  gameplay systems, MMO workflows, scripting providers, library adapters, platform integrations, authoring packs, diagnostics packs, migration packs, templates
```

Correct one-sentence claim:

```text
Saga's extension architecture is a manifest-driven Capability Package System where project-selected packages add runtime, server, editor, blocks, scripting, tooling, platform, migration, diagnostics, and library capabilities through deterministic SDE artifacts, SagaWeaver source-preserving metadata, policy-checked package manifests, lockfiles, and binary-specific module activation.
```

Engineering commandment:

```text
Do not make SagaCore bigger to make Saga feel powerful.
Make the package architecture strong enough that SagaCore can stay small.
```

Editor commandment:

```text
Do not force every enabled capability onto the screen.
Enabled package, available authoring surface, and visible panel are separate states.
```

Scripting commandment:

```text
Do not sacrifice C# source trust for scripting convenience.
Alternative languages are project capabilities, not replacements for SagaWeaver.
```

Runtime commandment:

```text
Editor extensibility may be dynamic; runtime and server hot paths must remain typed, compiled, and diagnosable.
```

Migration commandment:

```text
A package that owns persistent data owns explicit migration responsibility.
```

Boundary commandment:

```text
Core depends on stable contracts.
Packages depend on core contracts.
Core must not depend on packages.
```
