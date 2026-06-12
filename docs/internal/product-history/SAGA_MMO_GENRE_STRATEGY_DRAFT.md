# Saga MMO Genre Strategy Draft

> Historical draft only. This is not the current source of truth for Saga's
> product claims, repository layout, package/plugin APIs, or release scope.
> Use `docs/architecture/SAGA_MMO_GENRE_FOCUS.md` for the accepted concise
> decision. Treat proposed folders, C++ skeletons, package contracts, and 0.1
> scope in this draft as planning material unless another current architecture
> document explicitly accepts them.

# Saga Architecture Strategy v2 — MMO Genre Focus, Anti-Abstraction, File/Code Architecture, and Editor Collaboration

> **Status:** Architecture decision document / revision
> **Purpose:** Clarify Saga's MMO genre focus, unnecessary abstraction boundaries, the separation between Engine/Editor/Packages/Tools, the proposed file architecture, and example C++ code skeletons.
> **Main decision:** Saga's core provides genre-neutral MMO primitives; genre-specific implementations live as packages/plugins. Editor collaboration is not an MMO genre; it is a product/platform layer.

---

## 0. Reason for This Revision

The previous document made the correct primary decision:

```text
Saga = persistent sandbox MMO engine.
Primary focus = Persistent Survival Sandbox MMO.
Secondary focus = Social Sandbox / Player-driven Economy MMO.
Long-term direction = Creator-driven persistent MMO worlds.
```

However, several areas were still under-specified:

```text
- The file architecture was not layered enough.
- The question "Will everything be forced into the Engine directory?" was not answered clearly.
- Editor collaboration could be mistaken for a game genre.
- There were not enough examples of what the code should look like.
- Package/plugin boundaries were not clarified with C++ interfaces and registration examples.
- Runtime hot-path code and editor/tooling cold-path code were not separated explicitly.
```

The purpose of this revision is not to create a perfect design. The purpose is to **avoid taking on the wrong debt at the beginning**.

---

## 1. Short and Precise Product Statement

Saga's long-term product statement:

```text
Saga is a persistent sandbox MMO engine that prioritizes survival and social sandbox MMO worlds.
Its core focuses on genre-neutral MMO primitives; MMORPG, MMOFPS, MMORTS, and other genres
can be added as packages/plugins, but their gameplay implementations are not embedded into the core.
```

Shorter version:

```text
Small core.
Fast runtime.
Flexible editor.
Genres as packages.
Collaboration as product layer.
MMORPG is possible, but not core.
```

---

## 2. MMO Genres Saga Should Prioritize

### 2.1 Primary Focus

```text
Persistent Survival Sandbox MMO
```

This is Saga's first real technical validation target.

This genre forces the engine to solve the following problems:

```text
- persistent world state
- entity persistence
- item identity
- inventory replication
- base-building
- ownership and permissions
- region/land control
- guild/clan ownership
- resource spawning
- crafting data flow
- server-authoritative validation
- interest management
- world partitioning
- dedicated server workflow
```

For that reason, Saga's 0.1 Technical Preview target should be a **survival sandbox vertical slice**.

---

### 2.2 Secondary Focus

```text
Social Sandbox MMO / Player-driven Economy MMO
```

This is not a separate core architecture. It is a natural layer above the survival sandbox foundation.

Shared infrastructure:

```text
- OwnerId
- GroupId
- PermissionPolicy
- PersistentEntity
- SharedStorage
- Trade-safe transaction
- EconomyLedger
- LandClaim
- RegionGovernance
```

Some of these belong in core primitives; others belong in package implementations.

---

### 2.3 Long-Term Platform Direction

```text
Creator-driven persistent MMO worlds
```

This is not a runtime genre. It is a **platform/tooling direction**.

Incorrect statement:

```text
Saga's editor will be an MMO genre.
```

Correct statement:

```text
Saga's editor will provide a collaborative product layer for creating multiplayer MMO worlds.
```

The editor-side collaboration system is not a game genre. It is not a player gameplay system. It must not be confused with the engine runtime's MMO genre focus.

---

## 3. What Editor Collaboration Is and Is Not

### 3.1 What It Is Not

Editor collaboration is not any of the following:

```text
- It is not an MMORPG.
- It is not a Survival MMO.
- It is not a Social Sandbox MMO.
- It is not a gameplay genre of the engine runtime.
- It is not part of the MMO world played by players.
```

### 3.2 What It Is

Editor collaboration is:

```text
A product/platform layer that allows multiple developers to work on the same Saga project.
```

Example features:

```text
- multi-user access to the same project
- asset lock / scene lock
- live presence
- comments and review system
- change history
- publish approval flow
- package version review
- role-based editor permissions
- team workspace
```

### 3.3 First Product Level

The first product level should remain small:

```text
Product Collaboration v1:
- project workspace
- user presence
- file/asset lock
- simple roles: owner/editor/viewer
- change history
- package publish confirmation
```

### 3.4 Enterprise Evolution

Enterprise-level capabilities come later:

```text
Enterprise Collaboration:
- SSO
- advanced RBAC
- audit log
- branch/merge workflow
- approval pipeline
- studio policy enforcement
- on-prem/self-host support
- compliance export
- per-team package registry
```

### 3.5 Critical Boundary

```text
Editor collaboration is not embedded into the Engine runtime.
Editor collaboration is a separate product service/layer.
```

Correct location:

```text
Editor/
Services/
Tools/
```

Incorrect location:

```text
Engine/Runtime/World/EditorMultiplayerMMOSystem.cpp
```

---

## 4. Will Everything Be Forced Into the Engine Directory?

No.

`Engine/` should contain only the reusable engine core and runtime/editor contracts.

The Saga repository should be separated into the following parts:

```text
Engine    = engine core and public runtime/editor extension contracts
Editor    = user-facing editor application
Servers   = dedicated game server host and collaboration service host
Packages  = official gameplay/domain packages
Tools     = CLI tools and offline pipelines
Samples   = sample projects
Docs      = architecture decisions, guides, ADRs
Vendor    = vendored third-party sources
cmake     = build infrastructure
```

Rule:

```text
Engine is a foundation for other layers.
But Engine does not absorb everything.
```

---

## 5. Recommended Top-Level Repository Architecture

```text
Saga/
├─ Engine/
│  ├─ Public/
│  │  └─ SagaEngine/
│  │     ├─ Core/
│  │     ├─ Runtime/
│  │     ├─ World/
│  │     ├─ Network/
│  │     ├─ Persistence/
│  │     ├─ Authority/
│  │     ├─ Ownership/
│  │     ├─ Packages/
│  │     ├─ Plugins/
│  │     ├─ EditorContracts/
│  │     └─ Graphics/
│  │
│  ├─ Private/
│  │  ├─ Core/
│  │  ├─ Runtime/
│  │  ├─ World/
│  │  ├─ Network/
│  │  ├─ Persistence/
│  │  ├─ Authority/
│  │  ├─ Ownership/
│  │  ├─ Packages/
│  │  ├─ Plugins/
│  │  └─ Graphics/
│  │
│  └─ Tests/
│     ├─ Runtime/
│     ├─ World/
│     ├─ Network/
│     ├─ Persistence/
│     ├─ Authority/
│     ├─ Ownership/
│     ├─ Packages/
│     └─ Boundary/
│
├─ Editor/
│  ├─ Public/
│  │  └─ SagaEditor/
│  ├─ Private/
│  │  ├─ Shell/
│  │  ├─ Panels/
│  │  ├─ ProjectSystem/
│  │  ├─ VisualBlocks/
│  │  ├─ CSharpBridge/
│  │  ├─ PackageBrowser/
│  │  ├─ CollaborationClient/
│  │  └─ Diagnostics/
│  └─ Tests/
│
├─ Servers/
│  ├─ SagaDedicatedServer/
│  │  ├─ Private/
│  │  └─ Tests/
│  │
│  └─ SagaCollaborationServer/
│     ├─ Private/
│     └─ Tests/
│
├─ Packages/
│  ├─ Saga.Inventory/
│  ├─ Saga.Building/
│  ├─ Saga.Crafting/
│  ├─ Saga.Survival/
│  ├─ Saga.Guilds/
│  ├─ Saga.Economy/
│  ├─ Saga.SocialSandbox/
│  └─ Saga.ThemeparkMMORPG.Sample/
│
├─ Tools/
│  ├─ SagaPackager/
│  ├─ SagaStateCheck/
│  ├─ SagaChaosLab/
│  ├─ SagaProbe/
│  ├─ SagaDocGuard/
│  └─ SagaWeaver/
│
├─ Samples/
│  ├─ PersistentSurvivalSandbox/
│  ├─ SocialVillageSandbox/
│  ├─ DedicatedServerSandbox/
│  └─ ThemeparkMMORPGPluginSample/
│
├─ Docs/
│  ├─ Architecture/
│  ├─ ADR/
│  ├─ Runtime/
│  ├─ Packages/
│  ├─ Editor/
│  ├─ Collaboration/
│  ├─ PluginContracts/
│  └─ Performance/
│
├─ Vendor/
├─ cmake/
├─ conanfile.py
├─ CMakeLists.txt
└─ README.md
```

---

## 6. Dependency Direction

The most important architectural rule:

```text
Dependencies must not flow from lower layers upward.
External layers depend on Engine public contracts.
```

Correct dependency direction:

```text
Packages  ─┐
Editor    ─┼──> Engine/Public
Servers   ─┤
Tools     ─┘

Samples ───> Engine/Public + Packages/Public
```

Incorrect dependency direction:

```text
Engine ───> Editor
Engine ───> Samples
Engine ───> Saga.Survival
Engine ───> Saga.ThemeparkMMORPG
Engine ───> SagaCollaborationServer
```

Engine must never know about:

```text
- QuestSystem
- HungerSystem
- EditorPresencePanel
- TeamWorkspaceService
- PersistentSurvivalSandbox sample
```

Engine only provides contracts.

---

## 7. What Should Live Inside Engine?

The following belong inside Engine:

```text
- low-level core types
- world lifecycle
- entity/component primitives
- server authority primitives
- replication primitives
- interest management
- persistence transaction primitives
- ownership/permission primitives
- package/plugin registration contracts
- editor extension contracts
- rendering backend abstraction
- platform abstraction
```

The following do not belong inside Engine:

```text
- ready-made survival gameplay systems
- ready-made MMORPG quest/class/raid systems
- ready-made economy market systems
- editor UI panel implementations
- collaboration server implementation
- sample game logic
- studio workflow policy
```

---

## 8. Systems That May Belong in Core

```text
Engine/Public/SagaEngine/Core
- Result
- ErrorCode
- StableId
- StringId
- Span/View utilities
- Allocator interfaces
```

```text
Engine/Public/SagaEngine/World
- World
- WorldContext
- WorldTick
- WorldSystem
- WorldPartition
- RegionId
- ZoneId
- EntityId
```

```text
Engine/Public/SagaEngine/Network
- NetworkMessage
- ReplicationView
- Snapshot
- DeltaWriter
- InterestQuery
- ClientConnectionId
```

```text
Engine/Public/SagaEngine/Persistence
- PersistentId
- PersistenceWriter
- PersistenceReader
- SaveTransaction
- LoadTransaction
- SchemaVersion
```

```text
Engine/Public/SagaEngine/Authority
- PlayerCommand
- CommandValidationResult
- AuthorityContext
- CommandBuffer
```

```text
Engine/Public/SagaEngine/Ownership
- OwnerId
- GroupId
- PermissionPolicy
- AccessRule
```

```text
Engine/Public/SagaEngine/Packages
- PackageDescriptor
- PackageRegistry
- PackageDependency
```

```text
Engine/Public/SagaEngine/Plugins
- IRuntimePlugin
- IWorldSystem
- IPersistentSystem
- IReplicatedSystem
```

```text
Engine/Public/SagaEngine/EditorContracts
- IEditorExtension
- IInspectorProvider
- IGraphNodeProvider
- IPackageEditorExtension
```

---

## 9. What Should Live Inside a Package?

A package provides a specific gameplay/domain area.

Example:

```text
Packages/Saga.Building/
├─ Public/
│  └─ SagaPackages/Building/
│     ├─ BuildingPackage.h
│     ├─ BuildingComponent.h
│     ├─ BuildingPlacementSystem.h
│     └─ BuildingPermissionRules.h
│
├─ Private/
│  ├─ BuildingPackage.cpp
│  ├─ BuildingPlacementSystem.cpp
│  └─ BuildingPermissionRules.cpp
│
├─ Editor/
│  ├─ BuildingPieceInspector.cpp
│  └─ BuildingPalettePanel.cpp
│
├─ Tests/
│  ├─ BuildingPlacementTests.cpp
│  └─ BuildingPermissionTests.cpp
│
└─ package.saga.json
```

A package should include only `Engine/Public` contracts.

Incorrect:

```cpp
#include <SagaEngine/Private/World/WorldStorage.h>
```

Correct:

```cpp
#include <SagaEngine/World/WorldContext.h>
#include <SagaEngine/Plugins/IWorldSystem.h>
#include <SagaEngine/Persistence/PersistenceWriter.h>
```

---

## 10. Code Architecture: ID Types

One of the first things to implement should be stable and strongly typed IDs.

Incorrect:

```cpp
using EntityId = uint64_t;
using PlayerId = uint64_t;
using ItemId = uint64_t;
```

This does not prevent compile-time confusion.

Better:

```cpp
#pragma once

#include <cstdint>

namespace Saga {

template <typename Tag>
class StrongId final {
public:
    constexpr StrongId() = default;
    explicit constexpr StrongId(std::uint64_t value) : m_value(value) {}

    [[nodiscard]] constexpr std::uint64_t Value() const noexcept {
        return m_value;
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return m_value != 0;
    }

    friend constexpr bool operator==(StrongId lhs, StrongId rhs) noexcept {
        return lhs.m_value == rhs.m_value;
    }

private:
    std::uint64_t m_value = 0;
};

struct EntityIdTag;
struct PlayerIdTag;
struct ItemIdTag;
struct RegionIdTag;
struct GroupIdTag;

using EntityId = StrongId<EntityIdTag>;
using PlayerId = StrongId<PlayerIdTag>;
using ItemId   = StrongId<ItemIdTag>;
using RegionId = StrongId<RegionIdTag>;
using GroupId  = StrongId<GroupIdTag>;

} // namespace Saga
```

This looks small, but it prevents serious long-term bugs.

---

## 11. Code Architecture: World System Contract

Saga core must not know genres. It should only know system lifecycle.

```cpp
#pragma once

namespace Saga {

class WorldContext;
class WorldTickContext;

class IWorldSystem {
public:
    virtual ~IWorldSystem() = default;

    virtual void OnRegister(WorldContext& world) = 0;
    virtual void OnUnregister(WorldContext& world) = 0;
    virtual void Tick(WorldTickContext& tick) = 0;
};

} // namespace Saga
```

A package can then implement its own system:

```cpp
#pragma once

#include <SagaEngine/Plugins/IWorldSystem.h>

namespace Saga::Packages::Building {

class BuildingPlacementSystem final : public Saga::IWorldSystem {
public:
    void OnRegister(Saga::WorldContext& world) override;
    void OnUnregister(Saga::WorldContext& world) override;
    void Tick(Saga::WorldTickContext& tick) override;
};

} // namespace Saga::Packages::Building
```

Core does not know `BuildingPlacementSystem`. It only knows `IWorldSystem`.

---

## 12. Code Architecture: Runtime Plugin Contract

Package/plugin registration should be explicit.

```cpp
#pragma once

namespace Saga {

class PackageRegistry;
class WorldSystemRegistry;
class EditorExtensionRegistry;

class IRuntimePlugin {
public:
    virtual ~IRuntimePlugin() = default;

    virtual const char* GetPluginId() const noexcept = 0;
    virtual void RegisterPackage(PackageRegistry& packages) = 0;
    virtual void RegisterRuntimeSystems(WorldSystemRegistry& systems) = 0;
    virtual void RegisterEditorExtensions(EditorExtensionRegistry& editor) = 0;
};

} // namespace Saga
```

Example survival package:

```cpp
#include <SagaEngine/Plugins/IRuntimePlugin.h>
#include <SagaEngine/Packages/PackageRegistry.h>
#include <SagaEngine/World/WorldSystemRegistry.h>

#include "ResourceNodeSystem.h"
#include "CraftingSystem.h"
#include "BuildingPlacementSystem.h"

namespace Saga::Packages::Survival {

class SurvivalPlugin final : public Saga::IRuntimePlugin {
public:
    const char* GetPluginId() const noexcept override {
        return "saga.package.survival";
    }

    void RegisterPackage(Saga::PackageRegistry& packages) override {
        packages.Register({
            .id = "saga.package.survival",
            .version = "0.1.0"
        });
    }

    void RegisterRuntimeSystems(Saga::WorldSystemRegistry& systems) override {
        systems.Add<ResourceNodeSystem>();
        systems.Add<CraftingSystem>();
        systems.Add<BuildingPlacementSystem>();
    }

    void RegisterEditorExtensions(Saga::EditorExtensionRegistry& editor) override {
        // Editor-side extensions can be registered here if they exist.
        // The runtime hot path does not depend on these extensions.
    }
};

} // namespace Saga::Packages::Survival
```

---

## 13. Code Architecture: MMORPG Package Example

MMORPG support is not unnecessary. What is unnecessary is embedding MMORPG systems into Engine core.

Correct location:

```text
Packages/Saga.ThemeparkMMORPG.Sample/
```

Example:

```cpp
#pragma once

#include <SagaEngine/Plugins/IWorldSystem.h>
#include <SagaEngine/Persistence/IPersistentSystem.h>
#include <SagaEngine/Network/IReplicatedSystem.h>

namespace Saga::Packages::ThemeparkMMORPG {

class QuestSystem final
    : public Saga::IWorldSystem,
      public Saga::IPersistentSystem,
      public Saga::IReplicatedSystem {
public:
    void OnRegister(Saga::WorldContext& world) override;
    void OnUnregister(Saga::WorldContext& world) override;
    void Tick(Saga::WorldTickContext& tick) override;

    void Save(Saga::PersistenceWriter& writer) override;
    void Load(Saga::PersistenceReader& reader) override;

    void BuildReplicationView(Saga::ReplicationView& view) override;
};

} // namespace Saga::Packages::ThemeparkMMORPG
```

Engine core does not know this class.

Core only knows:

```text
IWorldSystem
IPersistentSystem
IReplicatedSystem
```

Therefore, Saga's message remains clear:

```text
MMORPG can be built.
But the MMORPG implementation is not core.
```

---

## 14. Code Architecture: Permission and Ownership Primitives

Ownership can belong in core because many genres use it.

```cpp
#pragma once

#include <SagaEngine/Core/StrongId.h>

namespace Saga {

enum class AccessAction {
    Read,
    Write,
    Build,
    Destroy,
    Trade,
    Invite,
    Manage
};

struct AccessRequest {
    PlayerId player;
    GroupId group;
    EntityId target;
    AccessAction action;
};

class PermissionPolicy {
public:
    [[nodiscard]] bool Allows(const AccessRequest& request) const noexcept;

    void Grant(GroupId group, AccessAction action);
    void Revoke(GroupId group, AccessAction action);

private:
    // Prefer a compact permission bitset over string maps in runtime.
    // Details remain in the private implementation.
};

struct OwnershipComponent {
    PlayerId owner;
    GroupId group;
    PermissionPolicy permissions;
};

} // namespace Saga
```

This is appropriate for core because:

```text
- survival uses base ownership
- social sandbox uses guild/land claim
- MMORPG can use party/guild storage
- MMOFPS can use squad ownership
- MMORTS can use alliance/base ownership
```

---

## 15. Code Architecture: Player Commands and Authority

What the client says must not be accepted as truth.

Incorrect:

```cpp
client.PlaceBuilding(pieceId, position);
world.Spawn(pieceId, position);
```

Correct:

```cpp
struct PlaceBuildingCommand {
    PlayerId player;
    EntityId previewEntity;
    RegionId region;
    float x;
    float y;
    float z;
};

enum class CommandRejectReason {
    None,
    InvalidPlayer,
    InvalidRegion,
    NotEnoughPermission,
    CollisionBlocked,
    MissingResources,
    RateLimited
};

struct CommandValidationResult {
    bool accepted = false;
    CommandRejectReason reason = CommandRejectReason::None;
};

class IAuthorityRule {
public:
    virtual ~IAuthorityRule() = default;
    virtual CommandValidationResult Validate(
        const PlaceBuildingCommand& command,
        const AuthorityContext& context) const = 0;
};
```

The package adds its own rule:

```cpp
class BuildingAuthorityRule final : public Saga::IAuthorityRule {
public:
    Saga::CommandValidationResult Validate(
        const Saga::PlaceBuildingCommand& command,
        const Saga::AuthorityContext& context) const override {
        // Permission, collision, resource, and region checks are performed here.
        return {.accepted = true};
    }
};
```

This structure fits Saga's survival/social sandbox focus while avoiding a core that is locked to survival only.

---

## 16. Code Architecture: Replication Dirty Marking

Do not rely on a generic event bus in the hot path.

Incorrect:

```cpp
EventBus.Emit("InventoryChanged", payload);
```

Correct:

```cpp
class ReplicationTracker {
public:
    void MarkDirty(EntityId entity, ComponentId component) noexcept;
    void ClearDirty(EntityId entity, ComponentId component) noexcept;
    bool IsDirty(EntityId entity, ComponentId component) const noexcept;
};
```

Package systems call this explicitly:

```cpp
void InventorySystem::AddItem(EntityId inventoryOwner, ItemId item) {
    // Inventory state is updated here.
    m_replication.MarkDirty(inventoryOwner, InventoryComponent::ComponentTypeId);
}
```

This is less flashy, but architecturally correct.

---

## 17. Code Architecture: Persistence Transaction

Persistence belongs in core. However, the "crafting recipe save format" does not belong in core.

```cpp
class PersistenceWriter {
public:
    void BeginEntity(EntityId entity);
    void WriteUInt64(const char* field, std::uint64_t value);
    void WriteString(const char* field, std::string_view value);
    void EndEntity();
};

class IPersistentSystem {
public:
    virtual ~IPersistentSystem() = default;
    virtual void Save(PersistenceWriter& writer) = 0;
    virtual void Load(PersistenceReader& reader) = 0;
};
```

A package uses this contract:

```cpp
void QuestSystem::Save(Saga::PersistenceWriter& writer) {
    // Quest state belongs to the package.
    // Writer is a core contract.
}
```

Core does not know quests. It only knows the serialization contract.

---

## 18. Code Architecture: Editor Extension Contract

The editor can be flexible, but the Engine runtime must not depend on it.

```cpp
#pragma once

namespace Saga::Editor {

class InspectorRegistry;
class GraphNodeRegistry;
class PanelRegistry;

class IEditorExtension {
public:
    virtual ~IEditorExtension() = default;

    virtual const char* GetExtensionId() const noexcept = 0;
    virtual void RegisterInspectors(InspectorRegistry& inspectors) = 0;
    virtual void RegisterGraphNodes(GraphNodeRegistry& graphs) = 0;
    virtual void RegisterPanels(PanelRegistry& panels) = 0;
};

} // namespace Saga::Editor
```

Example building package editor extension:

```cpp
class BuildingEditorExtension final : public Saga::Editor::IEditorExtension {
public:
    const char* GetExtensionId() const noexcept override {
        return "saga.editor.extension.building";
    }

    void RegisterInspectors(Saga::Editor::InspectorRegistry& inspectors) override {
        inspectors.Register<BuildingComponent>("Building Component Inspector");
    }

    void RegisterGraphNodes(Saga::Editor::GraphNodeRegistry& graphs) override {
        graphs.RegisterNode("Place Building Piece");
    }

    void RegisterPanels(Saga::Editor::PanelRegistry& panels) override {
        panels.RegisterPanel("Building Palette");
    }
};
```

This is an editor-side extension. Server tick does not depend on it.

---

## 19. Editor Collaboration Architecture

Editor collaboration should be a separate service/layer.

```text
Editor/Private/CollaborationClient
- PresenceClient
- AssetLockClient
- ChangeFeedClient
- WorkspaceSessionClient

Servers/SagaCollaborationServer
- WorkspaceSessionService
- PresenceService
- AssetLockService
- ReviewService
- AuditLogService
```

First product level:

```cpp
struct WorkspaceUser {
    UserId id;
    std::string displayName;
};

struct AssetLock {
    AssetId asset;
    UserId lockedBy;
    Timestamp acquiredAt;
};

class ICollaborationClient {
public:
    virtual ~ICollaborationClient() = default;

    virtual void JoinWorkspace(ProjectId project, UserId user) = 0;
    virtual void LeaveWorkspace(ProjectId project, UserId user) = 0;

    virtual bool TryLockAsset(AssetId asset, UserId user) = 0;
    virtual void UnlockAsset(AssetId asset, UserId user) = 0;

    virtual std::vector<WorkspaceUser> GetOnlineUsers(ProjectId project) const = 0;
};
```

This system is not Engine runtime.

Because:

```text
- it is not the same as game-world replication
- it is not the same as player authority
- it is not the same as gameplay persistence
- it is editor product experience and team workflow infrastructure
```

Short name:

```text
Editor Collaboration Layer
```

Incorrect name:

```text
Editor MMO System
```

---

## 20. Package Manifest Structure

Each package should be described by its own manifest.

```json
{
  "schemaVersion": 1,
  "id": "saga.package.building",
  "name": "Saga Building Package",
  "version": "0.1.0",
  "dependsOn": [
    "saga.engine.runtime",
    "saga.engine.persistence",
    "saga.engine.ownership"
  ],
  "runtimeSystems": [
    "BuildingPlacementSystem"
  ],
  "authorityRules": [
    "BuildingAuthorityRule"
  ],
  "persistentSystems": [
    "BuildingPersistenceSystem"
  ],
  "editorExtensions": [
    "BuildingEditorExtension"
  ]
}
```

MMORPG sample manifest:

```json
{
  "schemaVersion": 1,
  "id": "saga.package.themepark_mmorpg.sample",
  "name": "Saga Themepark MMORPG Sample Package",
  "version": "0.1.0",
  "dependsOn": [
    "saga.engine.runtime",
    "saga.engine.persistence",
    "saga.engine.network",
    "saga.engine.plugins"
  ],
  "runtimeSystems": [
    "QuestSystem",
    "DialogueSystem",
    "DungeonInstanceSystem"
  ],
  "editorExtensions": [
    "QuestGraphEditorExtension",
    "DialogueEditorExtension"
  ]
}
```

These manifests do not put a genre system into engine core. They only tell the package loader what to load.

---

## 21. CMake Target Architecture

Target boundaries are as important as file layout.

Recommended targets:

```text
SagaEngineCore
SagaEngineRuntime
SagaEngineWorld
SagaEngineNetwork
SagaEnginePersistence
SagaEngineAuthority
SagaEngineOwnership
SagaEnginePlugins
SagaEngineEditorContracts

SagaEditorApp
SagaDedicatedServer
SagaCollaborationServer

SagaPackageInventory
SagaPackageBuilding
SagaPackageCrafting
SagaPackageSurvival
SagaPackageGuilds
SagaPackageEconomy
SagaPackageSocialSandbox
SagaPackageThemeparkMMORPGSample
```

Rule:

```text
SagaEngineRuntime does not link to SagaPackageSurvival.
SagaPackageSurvival links to SagaEngineRuntime.
```

Incorrect:

```cmake
target_link_libraries(SagaEngineRuntime PRIVATE SagaPackageSurvival)
```

Correct:

```cmake
target_link_libraries(SagaPackageSurvival PUBLIC SagaEngineRuntime SagaEnginePersistence SagaEngineOwnership)
```

If an editor package extension is needed:

```cmake
target_link_libraries(SagaPackageBuildingEditor PUBLIC SagaEngineEditorContracts SagaPackageBuilding)
```

---

## 22. Public/Private Include Rule

A public header exposes only public contracts.

Incorrect:

```cpp
// Private implementation leaks into a public header.
#include "Engine/Private/World/ChunkStorage.h"
```

Correct:

```cpp
#include <SagaEngine/World/WorldContext.h>
#include <SagaEngine/Core/StrongId.h>
```

Boundary tests should be written:

```text
- Packages cannot include Engine/Private
- Editor cannot include Engine/Private
- Samples cannot include Engine/Private
- Tools cannot include Engine/Private unless explicitly allowed for a test/tool target
```

These tests are boring, but they protect the engine.

---

## 23. Runtime Hot Path Rule

The following should be treated as forbidden in the hot path:

```text
- JSON parsing
- string property lookup
- generic Variant maps
- reflection-heavy per-entity access
- scripting boundary per entity/per tick
- event bus dependency for replication
- uncontrolled allocation
```

Hot-path example:

```cpp
void ReplicationSystem::Tick(const WorldTickContext& tick) {
    auto dirty = m_tracker.CollectDirtyComponents();

    for (const DirtyComponent& item : dirty) {
        ReplicationView view = m_interest.BuildView(item.entity);
        m_serializer.WriteDelta(view, item.entity, item.component);
    }
}
```

Cold-path example:

```cpp
void BuildingPieceInspector::Draw(EditorContext& editor) {
    auto metadata = editor.Reflection().GetType("BuildingPiece");
    editor.PropertyGrid().Draw(metadata);
}
```

This distinction must remain explicit.

---

## 24. Anti-Abstraction Rule v2

Before a system is allowed into core, ask these questions:

```text
1. Is this system a genre-neutral fundamental requirement for MMO runtime?
2. Is it directly related to persistence, replication, authority, ownership, or world lifecycle?
3. Can it be used as a primitive across Survival, Social Sandbox, MMORPG, MMOFPS, and MMORTS?
4. Can it run without forcing the hot path through dynamic strings, Variant, reflection, or event buses?
5. If this system were implemented as a package/plugin, would Engine core still be complete?
```

Decision rule:

```text
If the answer is "it can be written as a package and core remains complete," it does not enter core.
```

Examples:

```text
PermissionPolicy -> may enter core.
QuestSystem -> cannot enter core.
BuildingPlacementSystem -> package.
Placement validation primitive -> core or Authority contract.
MarketOrderSystem -> package.
Economy transaction primitive -> maybe core contract, domain package.
```

---

## 25. Tech Debt Risks and Early Mitigation

### 25.1 Coupling Editor Collaboration to Runtime Networking

Risk:

```text
If editor collaboration and game networking are mixed, both systems become damaged.
```

Mitigation:

```text
SagaCollaborationServer is separate.
SagaDedicatedServer is separate.
They may share low-level networking utilities, but their domains are not the same.
```

---

### 25.2 Adding the Package System Too Late

Risk:

```text
The survival system is first written into core, then later someone tries to move it into a package.
```

Mitigation:

```text
Even the first official gameplay implementation is written as a package.
```

---

### 25.3 Letting Editor Extensions Leak Into Runtime

Risk:

```text
The server build carries dependencies on editor panels.
```

Mitigation:

```text
SagaEngineEditorContracts is a separate target.
Editor extension targets are not linked into the server.
```

---

### 25.4 Making Everything a Generic Graph

Risk:

```text
Visual blocks look powerful, but the runtime becomes slow and difficult to debug.
```

Mitigation:

```text
Graphs belong on the editor side.
They compile down into typed runtime commands/systems.
```

---

### 25.5 Putting Everything in Engine/Public

Risk:

```text
The public API grows too large, making future changes difficult.
```

Mitigation:

```text
The public API stays small.
Private implementation may grow.
Boundary audits are performed.
```

---

## 26. Scope of the 0.1 Technical Preview

Correct target:

```text
Persistent Survival Sandbox vertical slice.
```

Minimum scope:

```text
Engine:
- World lifecycle
- EntityId/PlayerId/ItemId stable types
- WorldSystemRegistry
- ServerAuthority contract
- PersistenceWriter/Reader
- Replication dirty tracking
- Interest management v0
- Ownership/PermissionPolicy

Packages:
- Inventory package
- Building package
- Crafting package
- Survival sample package

Servers:
- Dedicated server host
- No full collaboration server yet

Editor:
- package browser
- simple inspector
- visual block prototype
- no enterprise collaboration yet

Collaboration:
- optional local workspace lock mock
- real-time multi-user later

Samples:
- PersistentSurvivalSandbox
- DedicatedServerSandbox
- tiny ThemeparkMMORPGPluginSample only to prove extension
```

This scope is enough. Anything beyond it becomes early product bloat.

---

## 27. Scope of 0.2 / Product Collaboration

```text
- project workspace
- user presence
- asset lock
- simple project roles
- change feed
- package publish review
```

At this stage, the editor starts becoming a product.

---

## 28. Scope of Enterprise Evolution

```text
- advanced RBAC
- SSO
- audit log
- approval workflows
- branch/merge
- studio package registry
- policy enforcement
- on-prem deployment
```

These are not core engine targets. They are product/platform targets.

---

## 29. The Most Critical Sentence to Remember

```text
Saga's Engine provides the runtime foundation for MMO game worlds.
Saga's Editor provides the experience for creating MMO worlds.
Saga's Collaboration layer is a team-work product.
These are parts of the same vision, but they are not the same layer.
```

---

## 30. Final Decision

Saga should do the following:

```text
- Build a strong persistent runtime core for survival/social sandbox MMOs.
- Keep the runtime core small and typed.
- Put genre-specific systems into packages/plugins.
- Make the editor flexible and package-aware.
- Design collaboration as a product layer.
- Add enterprise features later.
```

Saga should not do the following:

```text
- Represent every MMO genre inside core.
- Implement editor collaboration as if it were an MMO gameplay genre.
- Turn runtime into a visual graph engine.
- Move sample logic into Engine.
- Implement the survival system before the package system exists.
```

Shortest ruling:

```text
Engine core does not know the game genre.
Engine core knows how an MMO world operates.
Editor knows how teams create that world.
Packages know how genres behave.
Collaboration knows how teams work together.
```

---

## 31. Additional Critical Boundaries

This section exists to prevent Saga from taking on the wrong technical debt as it grows over the long term.

These rules are not meant to expand scope. They are meant to protect the existing architectural boundaries.

---

### 31.1 Source of Truth Rule

Every kind of state in Saga must have a single source of truth.

The same information must not be treated as truth simultaneously in the editor graph, C# file, runtime component, and manifest.

Correct approach:

```text
Source of truth for runtime world state:
- dedicated server runtime state

Source of truth for editor project state:
- project workspace / project database

Source of truth for package metadata:
- package.saga.json

Source of truth for build output:
- artifact manifest

Source of truth for C# / visual block authoring:
- explicitly defined by the source-preserving authoring model
```

Rule:

```text
A piece of data is either the primary source or a projection/cache/generated output.
It cannot be both at the same time.
```

Examples:

```text
A visual block graph may be a projection of C# source.
A runtime component is not the same thing as editor metadata.
An artifact manifest records build output; it is not the source asset itself.
```

This rule is especially critical for the C# ↔ Visual Blocks system.

---

### 31.2 C# and Visual Blocks Boundary

Visual blocks are not a runtime format.

Visual blocks are an authoring format.

Correct model:

```text
C# source / visual graph
        ↓
authoring validation
        ↓
typed runtime system / command / data
        ↓
server-authoritative execution
```

Incorrect model:

```text
Runtime executes visual graph nodes every frame.
```

This creates performance and debugging problems.

Rule:

```text
Visual blocks may be flexible on the editor/tooling side.
The runtime hot path must remain typed, explicit, and server-authoritative.
```

Core decision for C# ↔ Visual Blocks conversion:

```text
If block-compatible C# APIs are used, the graph may be editable.
If advanced C# is used, the graph may become read-only or be shown as an opaque block.
The graph is not the runtime itself.
```

---

### 31.3 Package Versioning and Compatibility

Every package must carry its own version and compatibility information.

Example:

```json
{
  "schemaVersion": 1,
  "id": "saga.package.building",
  "version": "0.1.0",
  "engineCompatibility": ">=0.1.0 <0.2.0",
  "saveSchemaVersion": 3,
  "networkSchemaVersion": 2
}
```

Rule:

```text
Packages should be updateable.
However, a package is not stable without a migration plan for save data, network protocol, and editor metadata.
```

A package is not only code. A package also carries:

```text
- runtime system information
- editor extension information
- save schema version
- network schema version
- engine compatibility range
- migration requirements
```

Without this, the package system breaks as it grows.

---

### 31.4 Save Migration Rule

Every component or system that writes persistent data must carry version information.

Problem example:

```text
0.1 BuildingComponent:
- owner
- position

0.2 BuildingComponent:
- owner
- group
- position
- durability
```

Question:

```text
How will an old world snapshot be opened in a newer engine version?
```

Therefore, every persistent system must define:

```text
- schema version
- migration path
- missing field behavior
- unsupported version behavior
- default value policy
```

Example interface:

```cpp
class IComponentMigration {
public:
    virtual ~IComponentMigration() = default;

    virtual SchemaVersion From() const = 0;
    virtual SchemaVersion To() const = 0;

    virtual void Migrate(PersistenceDocument& document) = 0;
};
```

Rule:

```text
A system that writes persistent data is also responsible for migration.
```

This is especially critical for MMOs because live world data should be treated as something that cannot simply be deleted.

---

### 31.5 Trust Boundary and Security

Saga must be server-authoritative.

Rule:

```text
The client is not trusted.
Plugins must not be treated as fully trusted by default.
C# scripts must not be treated as unlimited trusted code by default.
Editor users are not trusted by default.
Package manifests are not trusted until validated.
```

Therefore, the following boundaries must be explicit:

```text
Client command:
- validated by the server

Package:
- manifest is validated
- dependencies are checked
- access is limited to the allowed API surface

C# script:
- runs according to sandbox / allowed API boundaries

Editor collaboration:
- enforces workspace permissions

Server:
- remains the final authority
```

Incorrect approach:

```text
Give an item because the client said so.
Publish directly because an editor user changed something.
Trust a package manifest because it looks correct.
Allow scripts to access arbitrary engine internals.
```

Correct approach:

```text
Every external input is validated.
Every package is constrained.
Every publish/reload operation is checked.
Server state remains the primary authority.
```

---

### 31.6 Runtime Topology Separation

Saga's different server/service components are not the same thing.

They do not all have to be implemented at the beginning, but their conceptual separation should be correct from the start.

```text
SagaDedicatedServer:
- game simulation
- world tick
- replication
- authority
- persistence integration

SagaCollaborationServer:
- editor workspace
- user presence
- asset lock
- review/change feed
- team workflow

SagaGatewayServer:
- client connection routing
- session entry
- shard/zone routing

SagaAuthService:
- account/session validation

SagaPersistenceService:
- world save/load backend
- snapshot storage
- migration pipeline

SagaPackageRegistry:
- package metadata
- version compatibility
- package distribution
```

Critical separation:

```text
Game server networking != editor collaboration networking
Game persistence != editor workspace persistence
Player auth != editor team auth
```

Not all of these are required for 0.1.

For 0.1, the following is enough:

```text
- SagaDedicatedServer
- local/simple persistence
- package manifest validation
```

The collaboration server comes later as a product layer.

---

### 31.7 Observability and Diagnostics

An MMO engine must not run blind.

Every core subsystem must be debuggable and measurable.

The following should be observable:

```text
- server tick time
- replication bandwidth
- dirty component count
- interest management cost
- persistence transaction duration
- package/plugin load time
- system tick duration
- network message count
- authority rejection reason
- save/load result
```

Example diagnostics output:

```text
Replication.Tick: 4.2 ms
InterestManagement.BuildViews: 1.1 ms
SagaPackageBuilding.Tick: 0.8 ms
Persistence.SaveTransaction: 2.3 ms
Authority.RejectedCommands: 14
```

Rule:

```text
When the profiler shows something is slow, it should be clear which system/package/plugin caused it.
```

This prevents panic when performance problems appear.

---

### 31.8 Test Strategy

Unit tests alone are not enough for Saga.

Required test families:

```text
- public/private boundary tests
- package dependency tests
- plugin load/unload tests
- save/load golden snapshot tests
- migration tests
- network protocol compatibility tests
- authority rejection tests
- interest management visibility tests
- deterministic replay tests
- editor package extension tests
- collaboration lock conflict tests
- performance budget tests
```

Example performance budget:

```text
A 1000-entity replication test fails if it exceeds the defined budget.
```

Example authority test:

```text
A player without permission cannot delete another player's base part.
```

Example migration test:

```text
A 0.1 world snapshot can be opened with the 0.2 engine.
```

Rule:

```text
Catch technical debt with tests, not human memory.
```

---

### 31.9 Plugin Lifecycle and Hot Reload Boundary

The plugin system should be defined early, but trying to hot-reload everything is dangerous.

Decision for 0.1:

```text
No runtime hot unload.
The dedicated server package set is fixed at startup.
Limited package reload may exist on the editor side.
If a package carrying save data is disabled, the world must not open unsafely; it should either refuse to open or enter a degraded mode.
```

Incorrect goal:

```text
Every gameplay package can be unloaded/reloaded instantly while the server is running.
```

This is unnecessary and dangerous at the early stage.

Correct goal:

```text
Startup-time package registration.
Explicit plugin lifecycle.
Safe editor reload.
Runtime hot reload later.
```

Example lifecycle:

```cpp
class IRuntimePlugin {
public:
    virtual ~IRuntimePlugin() = default;

    virtual const char* GetPluginId() const noexcept = 0;

    virtual void RegisterPackage(PackageRegistry& packages) = 0;
    virtual void RegisterRuntimeSystems(WorldSystemRegistry& systems) = 0;
    virtual void RegisterEditorExtensions(EditorExtensionRegistry& editor) = 0;

    virtual bool CanUnloadRuntime() const noexcept {
        return false;
    }
};
```

Rule:

```text
A runtime plugin that carries save data or active world state cannot be hot-unloaded without a safe migration/unload plan.
```

---

### 31.10 Performance Budget Rule

Saga's performance targets do not need to start with perfect fixed numbers.

However, every subsystem should have a budget mindset.

Example budget areas:

```text
- server tick budget
- replication budget
- persistence save budget
- interest management budget
- package system tick budget
- editor graph validation budget
```

Rule:

```text
It is better to receive early warnings through performance budgets than to redesign the architecture after performance problems appear.
```

Example:

```text
If the 1000-entity replication test suddenly becomes 50% slower, it is investigated.
```

This is less about the exact number and more about detecting the trend.

---

## 32. Final Purpose of This Additional Section

This section is not meant to add a massive new scope to Saga.

Its purpose is:

```text
- avoid confusing the source of truth for state
- prevent visual blocks from leaking into runtime
- avoid growing the package system without versioning/migration
- avoid blindly trusting client/plugin/script/editor input
- avoid confusing the dedicated server and collaboration server
- make performance problems visible
- catch technical debt early through tests
```

Shortest decision:

```text
Saga must know not only what it does,
but also which layer each responsibility belongs to.
```
