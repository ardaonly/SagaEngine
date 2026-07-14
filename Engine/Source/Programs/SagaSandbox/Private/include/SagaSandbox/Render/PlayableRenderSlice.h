/// @file PlayableRenderSlice.h
/// @brief Deterministic single-object render scene used by sandbox and tests.

#pragma once

#include <SagaEngine/Math/Mat4.h>
#include <SagaEngine/Render/Backend/IRenderBackend.h>
#include <SagaEngine/Render/Materials/Material.h>
#include <SagaEngine/Render/Materials/MeshAsset.h>
#include <SagaEngine/Render/Scene/Camera.h>
#include <SagaEngine/Render/World/RenderWorld.h>

#include <array>
#include <cstdint>
#include <vector>

namespace SagaSandbox::Render
{

namespace EngineRender = ::SagaEngine::Render;
namespace Backend = ::SagaEngine::Render::Backend;
namespace Scene = ::SagaEngine::Render::Scene;
namespace World = ::SagaEngine::Render::World;

struct PlayableRenderSliceResources
{
    World::MeshId mesh = World::MeshId::kInvalid;
    World::MaterialId material = World::MaterialId::kInvalid;
    EngineRender::TextureHandle texture =
        EngineRender::TextureHandle::kInvalid;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return mesh != World::MeshId::kInvalid &&
               material != World::MaterialId::kInvalid;
    }
};

struct PlayableRenderSliceScene
{
    PlayableRenderSliceResources resources{};
    World::RenderEntityId entity = World::RenderEntityId::kInvalid;
    Scene::Camera camera{};

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return resources.IsValid() &&
               entity != World::RenderEntityId::kInvalid;
    }
};

[[nodiscard]] inline EngineRender::MeshAsset BuildPlayableCubeMeshAsset()
{
    EngineRender::MeshAsset asset{};
    asset.meshId = 1;
    asset.debugName = "PlayableSliceCube";
    asset.lodCount = 1;
    asset.rootBounds.centre = {0.0f, 0.0f, 0.0f};
    asset.rootBounds.halfExtents = {0.5f, 0.5f, 0.5f};

    auto& lod = asset.lods[0];

    auto pushFace = [&](EngineRender::MeshVec3 p0,
                        EngineRender::MeshVec3 p1,
                        EngineRender::MeshVec3 p2,
                        EngineRender::MeshVec3 p3,
                        EngineRender::MeshVec3 n,
                        EngineRender::MeshVec3 t)
    {
        const auto base = static_cast<std::uint32_t>(lod.vertices.size());

        auto makeVert = [&](EngineRender::MeshVec3 pos,
                            EngineRender::MeshVec2 uv)
        {
            EngineRender::MeshVertex v{};
            v.position = pos;
            v.normal = n;
            v.tangent = t;
            v.handedness = 1.0f;
            v.uv0 = uv;
            return v;
        };

        lod.vertices.push_back(makeVert(p0, {0.0f, 1.0f}));
        lod.vertices.push_back(makeVert(p1, {1.0f, 1.0f}));
        lod.vertices.push_back(makeVert(p2, {1.0f, 0.0f}));
        lod.vertices.push_back(makeVert(p3, {0.0f, 0.0f}));

        lod.indices.push_back(base);
        lod.indices.push_back(base + 1u);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base + 3u);
    };

    constexpr float h = 0.5f;
    pushFace({-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h},
             {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h},
             {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f});
    pushFace({h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h},
             {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
    pushFace({-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h},
             {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    pushFace({-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h},
             {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h},
             {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});

    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint = static_cast<std::uint32_t>(lod.indices.size());
    lod.approxGpuBytes =
        static_cast<std::uint64_t>(lod.vertices.size()) *
            sizeof(EngineRender::MeshVertex) +
        static_cast<std::uint64_t>(lod.indices.size()) *
            sizeof(std::uint32_t);
    return asset;
}

[[nodiscard]] inline std::vector<std::uint8_t>
BuildPlayableCheckerTexturePixels()
{
    constexpr std::uint32_t kTexSize = 64u;
    constexpr std::uint32_t kCellSize = 8u;
    std::vector<std::uint8_t> pixels(kTexSize * kTexSize * 4u);

    for (std::uint32_t y = 0; y < kTexSize; ++y)
    {
        for (std::uint32_t x = 0; x < kTexSize; ++x)
        {
            const bool white =
                ((x / kCellSize) + (y / kCellSize)) % 2u == 0u;
            const std::uint8_t c = white ? 230u : 50u;
            const std::uint32_t idx = (y * kTexSize + x) * 4u;
            pixels[idx + 0u] = c;
            pixels[idx + 1u] = c;
            pixels[idx + 2u] = c;
            pixels[idx + 3u] = 255u;
        }
    }

    return pixels;
}

[[nodiscard]] inline EngineRender::MaterialRuntime
BuildPlayableMaterialRuntime(EngineRender::TextureHandle albedo) noexcept
{
    EngineRender::MaterialRuntime material{};
    material.materialId = 1u;
    material.renderQueue = EngineRender::MaterialRenderQueue::Opaque;
    material.cullMode = EngineRender::MaterialCullMode::Back;
    material.writesDepth = true;
    material.vectors[0] = {0.2f, 0.7f, 1.0f, 1.0f};
    material.textures[
        static_cast<std::size_t>(EngineRender::MaterialTextureSlot::Albedo)] =
            albedo;
    return material;
}

[[nodiscard]] inline PlayableRenderSliceResources
CreatePlayableRenderSliceResources(Backend::IRenderBackend& backend)
{
    PlayableRenderSliceResources resources{};

    resources.mesh = backend.CreateMesh(BuildPlayableCubeMeshAsset());
    if (resources.mesh == World::MeshId::kInvalid)
    {
        return resources;
    }

    const auto pixels = BuildPlayableCheckerTexturePixels();
    resources.texture = backend.CreateTexture(64u, 64u, pixels.data());

    resources.material = backend.CreateMaterial(
        BuildPlayableMaterialRuntime(resources.texture));
    if (resources.material == World::MaterialId::kInvalid)
    {
        if (resources.texture != EngineRender::TextureHandle::kInvalid)
        {
            backend.DestroyTexture(resources.texture);
            resources.texture = EngineRender::TextureHandle::kInvalid;
        }
        backend.DestroyMesh(resources.mesh);
        resources.mesh = World::MeshId::kInvalid;
    }

    return resources;
}

[[nodiscard]] inline Scene::Camera MakePlayableMainCamera() noexcept
{
    Scene::Camera camera{};
    camera.position = {0.0f, 0.0f, 3.0f};
    camera.view = ::SagaEngine::Math::Mat4::LookAtRH(
        camera.position,
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f});
    camera.projection = ::SagaEngine::Math::Mat4::PerspectiveRH_ZO(
        1.0472f,
        16.0f / 9.0f,
        0.1f,
        100.0f);
    camera.RecomputeViewProj();
    camera.RecomputeFrustum();
    return camera;
}

[[nodiscard]] inline World::RenderEntityId AddPlayableRenderEntity(
    World::RenderWorld& world,
    const PlayableRenderSliceResources& resources)
{
    if (!resources.IsValid())
    {
        return World::RenderEntityId::kInvalid;
    }

    World::RenderEntity entity{};
    entity.mesh = resources.mesh;
    entity.material = resources.material;
    entity.boundsRadius = 0.9f;
    entity.visible = true;
    entity.visibilityMask = World::kVisibilityAll;
    return world.Create(entity);
}

[[nodiscard]] inline PlayableRenderSliceScene CreatePlayableRenderSliceScene(
    Backend::IRenderBackend& backend,
    World::RenderWorld& world)
{
    PlayableRenderSliceScene scene{};
    scene.resources = CreatePlayableRenderSliceResources(backend);
    scene.entity = AddPlayableRenderEntity(world, scene.resources);
    scene.camera = MakePlayableMainCamera();
    return scene;
}

inline void DestroyPlayableRenderSliceResources(
    Backend::IRenderBackend& backend,
    PlayableRenderSliceResources& resources)
{
    if (resources.mesh != World::MeshId::kInvalid)
    {
        backend.DestroyMesh(resources.mesh);
        resources.mesh = World::MeshId::kInvalid;
    }
    if (resources.material != World::MaterialId::kInvalid)
    {
        backend.DestroyMaterial(resources.material);
        resources.material = World::MaterialId::kInvalid;
    }
    if (resources.texture != EngineRender::TextureHandle::kInvalid)
    {
        backend.DestroyTexture(resources.texture);
        resources.texture = EngineRender::TextureHandle::kInvalid;
    }
}

} // namespace SagaSandbox::Render
