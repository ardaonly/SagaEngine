/// @file StarterArenaPlayableScene.cpp
/// @brief Backend-agnostic visible scene owned by the StarterArena runtime app.

#include "StarterArenaPlayableScene.h"

#include <SagaEngine/Math/Mat4.h>
#include <SagaEngine/Math/Transform.h>
#include <SagaEngine/Render/Culling/CullingPipeline.h>
#include <SagaEngine/Render/Materials/MeshAsset.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace SagaRuntimeApp
{
namespace
{

using SagaEngine::Math::Transform;
using SagaEngine::Math::Vec3;
using SagaEngine::Render::MaterialCullMode;
using SagaEngine::Render::MaterialRenderQueue;
using SagaEngine::Render::MaterialRuntime;
using SagaEngine::Render::MaterialTextureSlot;
using SagaEngine::Render::MeshAsset;
using SagaEngine::Render::MeshVec2;
using SagaEngine::Render::MeshVec3;
using SagaEngine::Render::MeshVertex;
using SagaEngine::Render::TextureHandle;

MeshAsset BuildCubeMesh()
{
    MeshAsset asset{};
    asset.meshId = 0x53544101u;
    asset.debugName = "StarterArenaPlayerAndBoundaryCube";
    asset.lodCount = 1;
    asset.rootBounds.halfExtents = {0.5f, 0.5f, 0.5f};
    auto& lod = asset.lods[0];

    auto pushFace =
        [&](MeshVec3 p0, MeshVec3 p1, MeshVec3 p2, MeshVec3 p3, MeshVec3 normal, MeshVec3 tangent) {
            const auto base = static_cast<std::uint32_t>(lod.vertices.size());
            auto vertex = [&](MeshVec3 position, MeshVec2 uv) {
                MeshVertex value{};
                value.position = position;
                value.normal = normal;
                value.tangent = tangent;
                value.uv0 = uv;
                return value;
            };
            lod.vertices.push_back(vertex(p0, {0.0f, 1.0f}));
            lod.vertices.push_back(vertex(p1, {1.0f, 1.0f}));
            lod.vertices.push_back(vertex(p2, {1.0f, 0.0f}));
            lod.vertices.push_back(vertex(p3, {0.0f, 0.0f}));
            lod.indices.insert(lod.indices.end(),
                               {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
        };

    constexpr float h = 0.5f;
    pushFace(
        {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({h, -h, -h},
             {-h, -h, -h},
             {-h, h, -h},
             {h, h, -h},
             {0.0f, 0.0f, -1.0f},
             {-1.0f, 0.0f, 0.0f});
    pushFace(
        {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
    pushFace({-h, -h, -h},
             {-h, -h, h},
             {-h, h, h},
             {-h, h, -h},
             {-1.0f, 0.0f, 0.0f},
             {0.0f, 0.0f, 1.0f});
    pushFace(
        {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({-h, -h, -h},
             {h, -h, -h},
             {h, -h, h},
             {-h, -h, h},
             {0.0f, -1.0f, 0.0f},
             {1.0f, 0.0f, 0.0f});
    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint = static_cast<std::uint32_t>(lod.indices.size());
    return asset;
}

MeshAsset BuildGroundMesh()
{
    MeshAsset asset{};
    asset.meshId = 0x53544102u;
    asset.debugName = "StarterArenaGround";
    asset.lodCount = 1;
    asset.rootBounds.halfExtents = {0.5f, 0.01f, 0.5f};
    auto& lod = asset.lods[0];
    auto vertex = [](MeshVec3 position, MeshVec2 uv) {
        MeshVertex value{};
        value.position = position;
        value.normal = {0.0f, 1.0f, 0.0f};
        value.tangent = {1.0f, 0.0f, 0.0f};
        value.uv0 = uv;
        return value;
    };
    lod.vertices = {vertex({-0.5f, 0.0f, 0.5f}, {0.0f, 1.0f}),
                    vertex({0.5f, 0.0f, 0.5f}, {1.0f, 1.0f}),
                    vertex({0.5f, 0.0f, -0.5f}, {1.0f, 0.0f}),
                    vertex({-0.5f, 0.0f, -0.5f}, {0.0f, 0.0f})};
    lod.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    lod.vertexCountHint = 4u;
    lod.indexCountHint = 6u;
    return asset;
}

std::array<std::uint8_t, 4u * 4u * 4u> SolidPixels(std::uint8_t red,
                                                   std::uint8_t green,
                                                   std::uint8_t blue)
{
    std::array<std::uint8_t, 4u * 4u * 4u> pixels{};
    for (std::size_t index = 0; index < pixels.size(); index += 4u)
    {
        pixels[index] = red;
        pixels[index + 1u] = green;
        pixels[index + 2u] = blue;
        pixels[index + 3u] = 255u;
    }
    return pixels;
}

MaterialRuntime MakeMaterial(std::uint64_t id, TextureHandle texture)
{
    MaterialRuntime material{};
    material.materialId = id;
    material.renderQueue = MaterialRenderQueue::Opaque;
    material.cullMode = MaterialCullMode::Back;
    material.writesDepth = true;
    material.textures[static_cast<std::size_t>(MaterialTextureSlot::Albedo)] = texture;
    return material;
}

RenderWorld::RenderEntity MakeEntity(RenderWorld::MeshId mesh,
                                     RenderWorld::MaterialId material,
                                     Transform transform,
                                     float boundsRadius)
{
    RenderWorld::RenderEntity entity{};
    entity.mesh = mesh;
    entity.material = material;
    entity.transform = transform;
    entity.boundsRadius = boundsRadius;
    return entity;
}

} // namespace

bool StarterArenaPlayableResources::IsValid() const noexcept
{
    return cubeMesh != RenderWorld::MeshId::kInvalid &&
           groundMesh != RenderWorld::MeshId::kInvalid &&
           playerMaterial != RenderWorld::MaterialId::kInvalid &&
           groundMaterial != RenderWorld::MaterialId::kInvalid &&
           boundaryMaterial != RenderWorld::MaterialId::kInvalid &&
           playerTexture != TextureHandle::kInvalid && groundTexture != TextureHandle::kInvalid &&
           boundaryTexture != TextureHandle::kInvalid;
}

bool StarterArenaPlayableScene::IsValid() const noexcept
{
    return resources.IsValid() && ground != RenderWorld::RenderEntityId::kInvalid &&
           player != RenderWorld::RenderEntityId::kInvalid &&
           std::all_of(boundaries.begin(), boundaries.end(), [](auto entity) {
               return entity != RenderWorld::RenderEntityId::kInvalid;
           });
}

Vec3 StarterArenaWorldPosition(StarterArenaVec2 position, float height) noexcept
{
    return {static_cast<float>(position.x), height, static_cast<float>(position.y)};
}

RenderScene::Camera MakeStarterArenaPlayableCamera(const StarterArenaScene& scene,
                                                   std::uint32_t viewportWidth,
                                                   std::uint32_t viewportHeight) noexcept
{
    const float declaredWidth = static_cast<float>(scene.camera.width);
    const float declaredHeight = static_cast<float>(scene.camera.height);
    const float aspect = viewportHeight == 0u ? 1.0f
                                              : static_cast<float>(viewportWidth) /
                                                    static_cast<float>(viewportHeight);
    const float width = std::max(declaredWidth, declaredHeight * aspect);
    const float height = std::max(declaredHeight, declaredWidth / aspect);
    const float centerX = static_cast<float>(scene.camera.x);
    const float centerZ = static_cast<float>(scene.camera.y);

    RenderScene::Camera camera{};
    camera.position = {centerX, 5.0f, centerZ};
    camera.view = SagaEngine::Math::Mat4::LookAtRH(camera.position,
                                                   {centerX, 0.0f, centerZ},
                                                   {0.0f, 0.0f, -1.0f});
    camera.projection = SagaEngine::Math::Mat4::OrthoRH_ZO(
        -width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, 0.1f, 20.0f);
    camera.RecomputeViewProj();
    camera.RecomputeFrustum();
    return camera;
}

StarterArenaPlayableScene CreateStarterArenaPlayableScene(RenderBackend::IRenderBackend& backend,
                                                          RenderWorld::RenderWorld& world,
                                                          const StarterArenaScene& scene,
                                                          std::uint32_t viewportWidth,
                                                          std::uint32_t viewportHeight)
{
    StarterArenaPlayableScene result;
    result.camera = MakeStarterArenaPlayableCamera(scene, viewportWidth, viewportHeight);
    auto& resources = result.resources;

    resources.cubeMesh = backend.CreateMesh(BuildCubeMesh());
    resources.groundMesh = backend.CreateMesh(BuildGroundMesh());
    const auto playerPixels = SolidPixels(245u, 190u, 45u);
    const auto groundPixels = SolidPixels(42u, 92u, 65u);
    const auto boundaryPixels = SolidPixels(210u, 70u, 65u);
    resources.playerTexture = backend.CreateTexture(4u, 4u, playerPixels.data());
    resources.groundTexture = backend.CreateTexture(4u, 4u, groundPixels.data());
    resources.boundaryTexture = backend.CreateTexture(4u, 4u, boundaryPixels.data());
    resources.playerMaterial = backend.CreateMaterial(
        MakeMaterial(0x53544111u, resources.playerTexture));
    resources.groundMaterial = backend.CreateMaterial(
        MakeMaterial(0x53544112u, resources.groundTexture));
    resources.boundaryMaterial = backend.CreateMaterial(
        MakeMaterial(0x53544113u, resources.boundaryTexture));
    if (!resources.IsValid())
    {
        DestroyStarterArenaPlayableScene(backend, world, result);
        return result;
    }

    const float width = static_cast<float>(scene.bounds.maxX - scene.bounds.minX);
    const float depth = static_cast<float>(scene.bounds.maxY - scene.bounds.minY);
    const float centerX = static_cast<float>((scene.bounds.minX + scene.bounds.maxX) * 0.5);
    const float centerZ = static_cast<float>((scene.bounds.minY + scene.bounds.maxY) * 0.5);

    Transform groundTransform = Transform::FromPosition({centerX, 0.0f, centerZ});
    groundTransform.scale = {width, 1.0f, depth};
    result.ground = world.Create(MakeEntity(
        resources.groundMesh, resources.groundMaterial, groundTransform, std::max(width, depth)));

    Transform playerTransform = Transform::FromPosition(
        StarterArenaWorldPosition(scene.playerSpawn));
    playerTransform.scale = {0.24f, 0.8f, 0.24f};
    result.player = world.Create(
        MakeEntity(resources.cubeMesh, resources.playerMaterial, playerTransform, 0.5f));

    constexpr float thickness = 0.06f;
    constexpr float wallHeight = 0.12f;
    const std::array<Transform, 4> boundaryTransforms{
        Transform{{centerX, wallHeight, static_cast<float>(scene.bounds.minY)},
                  {},
                  {width + thickness, wallHeight, thickness}},
        Transform{{centerX, wallHeight, static_cast<float>(scene.bounds.maxY)},
                  {},
                  {width + thickness, wallHeight, thickness}},
        Transform{{static_cast<float>(scene.bounds.minX), wallHeight, centerZ},
                  {},
                  {thickness, wallHeight, depth + thickness}},
        Transform{{static_cast<float>(scene.bounds.maxX), wallHeight, centerZ},
                  {},
                  {thickness, wallHeight, depth + thickness}}};
    for (std::size_t index = 0; index < result.boundaries.size(); ++index)
    {
        result.boundaries[index] = world.Create(MakeEntity(resources.cubeMesh,
                                                           resources.boundaryMaterial,
                                                           boundaryTransforms[index],
                                                           std::max(width, depth)));
    }
    return result;
}

void UpdateStarterArenaPlayerTransform(RenderWorld::RenderWorld& world,
                                       RenderWorld::RenderEntityId player,
                                       StarterArenaVec2 position) noexcept
{
    RenderWorld::RenderEntity* entity = world.Get(player);
    if (entity == nullptr)
    {
        return;
    }
    entity->transform.position = StarterArenaWorldPosition(position);
}

RenderScene::RenderView BuildStarterArenaPlayableView(const RenderWorld::RenderWorld& world,
                                                      const RenderScene::Camera& camera)
{
    SagaEngine::Render::Culling::CullingPipeline culling;
    SagaEngine::Render::Culling::CullingConfig config;
    config.reserveHint = world.LiveCount();
    RenderScene::RenderView view;
    culling.Run(world, camera, config, view);
    return view;
}

bool ViewContainsEntity(const RenderScene::RenderView& view,
                        RenderWorld::RenderEntityId entity) noexcept
{
    return std::any_of(view.drawItems.begin(),
                       view.drawItems.end(),
                       [entity](const RenderScene::DrawItem& item) {
                           return item.entity == entity;
                       });
}

void DestroyStarterArenaPlayableScene(RenderBackend::IRenderBackend& backend,
                                      RenderWorld::RenderWorld& world,
                                      StarterArenaPlayableScene& scene) noexcept
{
    world.Destroy(scene.player);
    world.Destroy(scene.ground);
    for (const auto entity : scene.boundaries)
    {
        world.Destroy(entity);
    }
    scene.player = RenderWorld::RenderEntityId::kInvalid;
    scene.ground = RenderWorld::RenderEntityId::kInvalid;
    scene.boundaries.fill(RenderWorld::RenderEntityId::kInvalid);

    auto& resources = scene.resources;
    if (resources.playerMaterial != RenderWorld::MaterialId::kInvalid)
        backend.DestroyMaterial(resources.playerMaterial);
    if (resources.groundMaterial != RenderWorld::MaterialId::kInvalid)
        backend.DestroyMaterial(resources.groundMaterial);
    if (resources.boundaryMaterial != RenderWorld::MaterialId::kInvalid)
        backend.DestroyMaterial(resources.boundaryMaterial);
    if (resources.cubeMesh != RenderWorld::MeshId::kInvalid)
        backend.DestroyMesh(resources.cubeMesh);
    if (resources.groundMesh != RenderWorld::MeshId::kInvalid)
        backend.DestroyMesh(resources.groundMesh);
    if (resources.playerTexture != TextureHandle::kInvalid)
        backend.DestroyTexture(resources.playerTexture);
    if (resources.groundTexture != TextureHandle::kInvalid)
        backend.DestroyTexture(resources.groundTexture);
    if (resources.boundaryTexture != TextureHandle::kInvalid)
        backend.DestroyTexture(resources.boundaryTexture);
    resources = {};
}

} // namespace SagaRuntimeApp
