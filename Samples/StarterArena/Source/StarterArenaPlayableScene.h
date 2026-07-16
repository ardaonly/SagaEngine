// SPDX-License-Identifier: Apache-2.0

/// @file StarterArenaPlayableScene.h
/// @brief Backend-agnostic visible scene owned by the StarterArena runtime app.

#pragma once

#include "StarterArenaSmoke.h"

#include <SagaEngine/Math/Vec3.h>
#include <SagaEngine/Render/Backend/IRenderBackend.h>
#include <SagaEngine/Render/Materials/Material.h>
#include <SagaEngine/Render/Scene/Camera.h>
#include <SagaEngine/Render/Scene/RenderView.h>
#include <SagaEngine/Render/World/RenderWorld.h>
#include <array>
#include <cstdint>

namespace SagaRuntimeApp
{

namespace RenderBackend = SagaEngine::Render::Backend;
namespace RenderScene = SagaEngine::Render::Scene;
namespace RenderWorld = SagaEngine::Render::World;

struct StarterArenaPlayableResources
{
    RenderWorld::MeshId cubeMesh = RenderWorld::MeshId::kInvalid;
    RenderWorld::MeshId groundMesh = RenderWorld::MeshId::kInvalid;
    RenderWorld::MaterialId playerMaterial = RenderWorld::MaterialId::kInvalid;
    RenderWorld::MaterialId groundMaterial = RenderWorld::MaterialId::kInvalid;
    RenderWorld::MaterialId boundaryMaterial = RenderWorld::MaterialId::kInvalid;
    RenderWorld::MaterialId pickupMaterial = RenderWorld::MaterialId::kInvalid;
    RenderWorld::MaterialId poweredPlayerMaterial = RenderWorld::MaterialId::kInvalid;
    SagaEngine::Render::TextureHandle playerTexture = SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle groundTexture = SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle boundaryTexture = SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle pickupTexture = SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle poweredPlayerTexture = SagaEngine::Render::TextureHandle::kInvalid;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct StarterArenaPlayableScene
{
    StarterArenaPlayableResources resources;
    RenderWorld::RenderEntityId ground = RenderWorld::RenderEntityId::kInvalid;
    RenderWorld::RenderEntityId player = RenderWorld::RenderEntityId::kInvalid;
    RenderWorld::RenderEntityId pickup = RenderWorld::RenderEntityId::kInvalid;
    std::array<RenderWorld::RenderEntityId, 4> boundaries{RenderWorld::RenderEntityId::kInvalid,
                                                          RenderWorld::RenderEntityId::kInvalid,
                                                          RenderWorld::RenderEntityId::kInvalid,
                                                          RenderWorld::RenderEntityId::kInvalid};
    RenderScene::Camera camera;

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] SagaEngine::Math::Vec3 StarterArenaWorldPosition(StarterArenaVec2 position,
                                                               float height = 0.4f) noexcept;

[[nodiscard]] RenderScene::Camera MakeStarterArenaPlayableCamera(
    const StarterArenaScene& scene,
    std::uint32_t viewportWidth,
    std::uint32_t viewportHeight) noexcept;

[[nodiscard]] StarterArenaPlayableScene CreateStarterArenaPlayableScene(
    RenderBackend::IRenderBackend& backend,
    RenderWorld::RenderWorld& world,
    const StarterArenaScene& scene,
    std::uint32_t viewportWidth,
    std::uint32_t viewportHeight);

void UpdateStarterArenaPlayerTransform(RenderWorld::RenderWorld& world,
                                       RenderWorld::RenderEntityId player,
                                       StarterArenaVec2 position) noexcept;

void ApplyStarterArenaGameplayReflection(RenderWorld::RenderWorld& world,
                                         StarterArenaPlayableScene& scene,
                                         bool pickupCollected,
                                         bool powered) noexcept;

[[nodiscard]] RenderScene::RenderView BuildStarterArenaPlayableView(
    const RenderWorld::RenderWorld& world, const RenderScene::Camera& camera);

[[nodiscard]] bool ViewContainsEntity(const RenderScene::RenderView& view,
                                      RenderWorld::RenderEntityId entity) noexcept;

void DestroyStarterArenaPlayableScene(RenderBackend::IRenderBackend& backend,
                                      RenderWorld::RenderWorld& world,
                                      StarterArenaPlayableScene& scene) noexcept;

} // namespace SagaRuntimeApp
