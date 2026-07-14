/// @file First3DVerticalSliceScenario.cpp
/// @brief Playable 3D vertical slice for Diligent/Vulkan runtime smoke.

#include "Scenarios/First3DVerticalSliceScenario.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Input/Backends/IPlatformInputBackend.h>
#include <SagaEngine/Input/Devices/KeyboardDevice.h>
#include <SagaEngine/Input/Devices/MouseDevice.h>
#include <SagaEngine/Math/Quat.h>
#include <SagaEngine/Render/Materials/MeshAsset.h>
#include <SagaEngine/Render/Scene/Camera.h>
#include <SagaEngine/Render/Scene/RenderView.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(First3DVerticalSliceScenario);

namespace Input = SagaEngine::Input;
namespace Math = SagaEngine::Math;
namespace Scene = SagaEngine::Render::Scene;
namespace World = SagaEngine::Render::World;

using SagaEngine::Render::MaterialCullMode;
using SagaEngine::Render::MaterialRenderQueue;
using SagaEngine::Render::MaterialRuntime;
using SagaEngine::Render::MaterialTextureSlot;
using SagaEngine::Render::MeshAsset;
using SagaEngine::Render::MeshVec2;
using SagaEngine::Render::MeshVec3;
using SagaEngine::Render::MeshVertex;
using SagaEngine::Render::TextureHandle;

namespace
{

static constexpr const char* kTag = "First3DVerticalSlice";

MeshAsset BuildCubeMeshAsset()
{
    MeshAsset asset{};
    asset.meshId = 1;
    asset.debugName = "First3DVerticalSliceCube";
    asset.lodCount = 1;
    asset.rootBounds.centre = {0.0f, 0.0f, 0.0f};
    asset.rootBounds.halfExtents = {0.5f, 0.5f, 0.5f};

    auto& lod = asset.lods[0];
    auto pushFace = [&](MeshVec3 p0, MeshVec3 p1, MeshVec3 p2, MeshVec3 p3,
                        MeshVec3 n, MeshVec3 t)
    {
        const auto base = static_cast<std::uint32_t>(lod.vertices.size());
        auto makeVertex = [&](MeshVec3 position, MeshVec2 uv)
        {
            MeshVertex v{};
            v.position = position;
            v.normal = n;
            v.tangent = t;
            v.handedness = 1.0f;
            v.uv0 = uv;
            return v;
        };

        lod.vertices.push_back(makeVertex(p0, {0.0f, 1.0f}));
        lod.vertices.push_back(makeVertex(p1, {1.0f, 1.0f}));
        lod.vertices.push_back(makeVertex(p2, {1.0f, 0.0f}));
        lod.vertices.push_back(makeVertex(p3, {0.0f, 0.0f}));
        lod.indices.push_back(base + 0u);
        lod.indices.push_back(base + 1u);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base + 0u);
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
        static_cast<std::uint64_t>(lod.vertices.size()) * sizeof(MeshVertex) +
        static_cast<std::uint64_t>(lod.indices.size()) * sizeof(std::uint32_t);
    return asset;
}

MeshAsset BuildGroundMeshAsset()
{
    MeshAsset asset{};
    asset.meshId = 2;
    asset.debugName = "First3DVerticalSliceGround";
    asset.lodCount = 1;
    asset.rootBounds.centre = {0.0f, 0.0f, 0.0f};
    asset.rootBounds.halfExtents = {12.0f, 0.0f, 12.0f};

    auto& lod = asset.lods[0];
    constexpr float h = 12.0f;
    constexpr float uv = 12.0f;

    MeshVertex v0{}, v1{}, v2{}, v3{};
    v0.position = {-h, 0.0f,  h};
    v1.position = { h, 0.0f,  h};
    v2.position = { h, 0.0f, -h};
    v3.position = {-h, 0.0f, -h};
    v0.normal = v1.normal = v2.normal = v3.normal = {0.0f, 1.0f, 0.0f};
    v0.tangent = v1.tangent = v2.tangent = v3.tangent = {1.0f, 0.0f, 0.0f};
    v0.handedness = v1.handedness = v2.handedness = v3.handedness = 1.0f;
    v0.uv0 = {0.0f, uv};
    v1.uv0 = {uv, uv};
    v2.uv0 = {uv, 0.0f};
    v3.uv0 = {0.0f, 0.0f};

    lod.vertices = {v0, v1, v2, v3};
    lod.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    lod.vertexCountHint = 4u;
    lod.indexCountHint = 6u;
    return asset;
}

std::vector<std::uint8_t> BuildCheckerTexture(
    std::uint8_t r0, std::uint8_t g0, std::uint8_t b0,
    std::uint8_t r1, std::uint8_t g1, std::uint8_t b1)
{
    constexpr std::uint32_t kSize = 64u;
    constexpr std::uint32_t kCell = 8u;
    std::vector<std::uint8_t> pixels(kSize * kSize * 4u);
    for (std::uint32_t y = 0; y < kSize; ++y)
    {
        for (std::uint32_t x = 0; x < kSize; ++x)
        {
            const bool alt = ((x / kCell) + (y / kCell)) % 2u == 0u;
            const auto idx = (y * kSize + x) * 4u;
            pixels[idx + 0u] = alt ? r0 : r1;
            pixels[idx + 1u] = alt ? g0 : g1;
            pixels[idx + 2u] = alt ? b0 : b1;
            pixels[idx + 3u] = 255u;
        }
    }
    return pixels;
}

MaterialRuntime BuildMaterial(std::uint64_t id, TextureHandle albedo)
{
    MaterialRuntime material{};
    material.materialId = id;
    material.renderQueue = MaterialRenderQueue::Opaque;
    material.cullMode = MaterialCullMode::Back;
    material.writesDepth = true;
    material.textures[static_cast<std::size_t>(MaterialTextureSlot::Albedo)] =
        albedo;
    return material;
}

} // namespace

void First3DVerticalSliceScenario::OnAttach(const ScenarioContext& ctx)
{
    m_backend = ctx.renderBackend;
    m_requestClose = ctx.requestClose;
}

bool First3DVerticalSliceScenario::OnInit()
{
    LOG_INFO(kTag, "Initialising first 3D vertical slice.");
    if (!m_backend)
    {
        LOG_ERROR(kTag, "No render backend available.");
        return false;
    }

    auto inputBackend = SagaEngine::Platform::CreateSDLInputBackend();
    m_inputManager.SetBackend(std::move(inputBackend));
    if (!m_inputManager.Initialize())
    {
        LOG_ERROR(kTag, "InputManager init failed.");
        return false;
    }
    m_inputManager.RegisterDevice(Input::MakeKeyboardDevice(1));
    m_inputManager.RegisterDevice(Input::MakeMouseDevice(2));
    m_inputManager.SetCursorLocked(true);

    m_cubeMesh = m_backend->CreateMesh(BuildCubeMeshAsset());
    m_groundMesh = m_backend->CreateMesh(BuildGroundMeshAsset());
    if (m_cubeMesh == World::MeshId::kInvalid ||
        m_groundMesh == World::MeshId::kInvalid)
    {
        LOG_ERROR(kTag, "Mesh upload failed.");
        return false;
    }

    auto cubePixelsA = BuildCheckerTexture(50u, 120u, 255u, 230u, 240u, 255u);
    auto cubePixelsB = BuildCheckerTexture(230u, 80u, 60u, 80u, 210u, 120u);
    auto groundPixels = BuildCheckerTexture(48u, 54u, 58u, 95u, 105u, 110u);
    m_cubeTextureA = m_backend->CreateTexture(64u, 64u, cubePixelsA.data());
    m_cubeTextureB = m_backend->CreateTexture(64u, 64u, cubePixelsB.data());
    m_groundTexture = m_backend->CreateTexture(64u, 64u, groundPixels.data());

    m_cubeMaterialA = m_backend->CreateMaterial(BuildMaterial(1u, m_cubeTextureA));
    m_cubeMaterialB = m_backend->CreateMaterial(BuildMaterial(2u, m_cubeTextureB));
    m_groundMaterial = m_backend->CreateMaterial(BuildMaterial(3u, m_groundTexture));
    if (m_cubeMaterialA == World::MaterialId::kInvalid ||
        m_cubeMaterialB == World::MaterialId::kInvalid ||
        m_groundMaterial == World::MaterialId::kInvalid)
    {
        LOG_ERROR(kTag, "Material upload failed.");
        return false;
    }

    m_cubes = {
        {{0.0f, 0.75f, 0.0f}, {1.4f, 1.4f, 1.4f}, m_cubeMaterialA, 0.0f, 1.0f},
        {{-2.2f, 0.55f, -1.2f}, {1.0f, 1.0f, 1.0f}, m_cubeMaterialB, 0.4f, 0.0f},
        {{2.0f, 0.65f, -1.8f}, {1.2f, 1.2f, 1.2f}, m_cubeMaterialA, -0.5f, 0.35f},
        {{0.8f, 0.45f, 2.0f}, {0.8f, 0.8f, 0.8f}, m_cubeMaterialB, 0.0f, -0.45f},
    };

    return true;
}

void First3DVerticalSliceScenario::OnUpdate(float dt, uint64_t tick)
{
    m_inputManager.Update(static_cast<std::uint32_t>(tick));
    const auto& state = m_inputManager.GetMergedState();
    const auto& kb = state.keyboard;
    const auto& mouse = state.mouse;

    if (kb.WasJustPressed(Input::KeyCode::Escape) && m_requestClose)
    {
        m_requestClose();
    }

    if (kb.WasJustPressed(Input::KeyCode::Tab))
    {
        m_cursorLocked = !m_cursorLocked;
        m_inputManager.SetCursorLocked(m_cursorLocked);
    }

    if (m_cursorLocked)
    {
        constexpr float kMouseSensitivity = 0.003f;
        m_yaw += static_cast<float>(mouse.relX) * kMouseSensitivity;
        m_pitch -= static_cast<float>(mouse.relY) * kMouseSensitivity;
        m_pitch = std::clamp(m_pitch, -1.45f, 1.45f);
    }

    const float cy = std::cos(m_yaw);
    const float sy = std::sin(m_yaw);
    const float cp = std::cos(m_pitch);
    const float sp = std::sin(m_pitch);

    const Math::Vec3 forward{sy * cp, sp, -cy * cp};
    const Math::Vec3 right{cy, 0.0f, sy};

    float speed = 4.5f * dt;
    if (kb.IsDown(Input::KeyCode::LShift))
    {
        speed *= 2.2f;
    }

    if (kb.IsDown(Input::KeyCode::W)) m_cameraPosition += forward * speed;
    if (kb.IsDown(Input::KeyCode::S)) m_cameraPosition -= forward * speed;
    if (kb.IsDown(Input::KeyCode::D)) m_cameraPosition += right * speed;
    if (kb.IsDown(Input::KeyCode::A)) m_cameraPosition -= right * speed;
    if (kb.IsDown(Input::KeyCode::Space)) m_cameraPosition.y += speed;
    if (kb.IsDown(Input::KeyCode::LCtrl)) m_cameraPosition.y -= speed;

    for (auto& cube : m_cubes)
    {
        cube.rotation += cube.rotationSpeed * dt;
    }
}

void First3DVerticalSliceScenario::OnPrepareRender(
    Scene::Camera& outCamera,
    Scene::RenderView& outView)
{
    const Math::Vec3 target{
        m_cameraPosition.x + std::sin(m_yaw) * std::cos(m_pitch),
        m_cameraPosition.y + std::sin(m_pitch),
        m_cameraPosition.z - std::cos(m_yaw) * std::cos(m_pitch),
    };

    outCamera.position = m_cameraPosition;
    outCamera.view = Math::Mat4::LookAtRH(
        m_cameraPosition, target, {0.0f, 1.0f, 0.0f});
    outCamera.projection = Math::Mat4::PerspectiveRH_ZO(
        1.0472f, 16.0f / 9.0f, 0.1f, 150.0f);
    outCamera.RecomputeViewProj();
    outCamera.RecomputeFrustum();

    if (m_groundMesh != World::MeshId::kInvalid &&
        m_groundMaterial != World::MaterialId::kInvalid)
    {
        Scene::DrawItem ground{};
        ground.mesh = m_groundMesh;
        ground.material = m_groundMaterial;
        ground.model = Math::Mat4::Identity();
        outView.drawItems.push_back(ground);
    }

    for (const auto& cube : m_cubes)
    {
        Scene::DrawItem item{};
        item.mesh = m_cubeMesh;
        item.material = cube.material;

        const auto translation = Math::Mat4::FromTranslation(cube.position);
        const auto rotation = Math::Mat4::FromQuat(
            Math::Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, cube.rotation));
        const auto scale = Math::Mat4::FromScale(cube.scale);
        item.model = translation * rotation * scale;
        outView.drawItems.push_back(item);
    }
}

void First3DVerticalSliceScenario::OnRenderDebugUI()
{
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 120), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("First 3D Slice"))
    {
        ImGui::Text("Draw items: %zu", m_cubes.size() + 1u);
        ImGui::Text("Camera: %.1f %.1f %.1f",
                    m_cameraPosition.x,
                    m_cameraPosition.y,
                    m_cameraPosition.z);
        ImGui::Text("[WASD] move  [Mouse] look");
        ImGui::Text("[Tab] cursor  [Esc] quit");
    }
    ImGui::End();
}

void First3DVerticalSliceScenario::OnShutdown()
{
    if (m_backend)
    {
        if (m_cubeMaterialA != World::MaterialId::kInvalid)
            m_backend->DestroyMaterial(m_cubeMaterialA);
        if (m_cubeMaterialB != World::MaterialId::kInvalid)
            m_backend->DestroyMaterial(m_cubeMaterialB);
        if (m_groundMaterial != World::MaterialId::kInvalid)
            m_backend->DestroyMaterial(m_groundMaterial);
        if (m_cubeMesh != World::MeshId::kInvalid)
            m_backend->DestroyMesh(m_cubeMesh);
        if (m_groundMesh != World::MeshId::kInvalid)
            m_backend->DestroyMesh(m_groundMesh);
        if (m_cubeTextureA != TextureHandle::kInvalid)
            m_backend->DestroyTexture(m_cubeTextureA);
        if (m_cubeTextureB != TextureHandle::kInvalid)
            m_backend->DestroyTexture(m_cubeTextureB);
        if (m_groundTexture != TextureHandle::kInvalid)
            m_backend->DestroyTexture(m_groundTexture);
    }

    m_cubeMaterialA = World::MaterialId::kInvalid;
    m_cubeMaterialB = World::MaterialId::kInvalid;
    m_groundMaterial = World::MaterialId::kInvalid;
    m_cubeMesh = World::MeshId::kInvalid;
    m_groundMesh = World::MeshId::kInvalid;
    m_cubeTextureA = TextureHandle::kInvalid;
    m_cubeTextureB = TextureHandle::kInvalid;
    m_groundTexture = TextureHandle::kInvalid;
    m_cubes.clear();

    m_inputManager.SetCursorLocked(false);
    m_inputManager.Shutdown();
    LOG_INFO(kTag, "First 3D vertical slice shut down.");
}

} // namespace SagaSandbox
