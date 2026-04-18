/// @file RenderPlaygroundScenario.cpp
/// @brief Interactive cube scene exercising the full Phase 3 render pipeline.

#include "Scenarios/RenderPlaygroundScenario.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Input/Backends/SDL/SDLInputBackend.h>
#include <SagaEngine/Input/Devices/KeyboardDevice.h>
#include <SagaEngine/Input/Devices/MouseDevice.h>
#include <SagaEngine/Render/Materials/MeshAsset.h>
#include <SagaEngine/Render/Materials/Material.h>
#include <SagaEngine/Math/Quat.h>

#include <cmath>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(RenderPlaygroundScenario);

static constexpr const char* kTag = "RenderPlayground";

namespace Input = SagaEngine::Input;
namespace Math  = SagaEngine::Math;
namespace RB    = SagaEngine::Render::Backend;
namespace World = SagaEngine::Render::World;

using SagaEngine::Render::MeshAsset;
using SagaEngine::Render::MeshVertex;
using SagaEngine::Render::MeshVec3;
using SagaEngine::Render::MeshVec2;
using SagaEngine::Render::MaterialRuntime;
using SagaEngine::Render::MaterialRenderQueue;
using SagaEngine::Render::MaterialCullMode;

// ─── Procedural cube builder ─────────────────────────────────────────────────

namespace
{

/// Build a unit cube centred at origin with proper per-face normals.
/// 24 vertices (4 per face), 36 indices (2 triangles per face).
MeshAsset BuildCubeAsset()
{
    MeshAsset asset{};
    asset.meshId    = 1;
    asset.debugName = "ProceduralCube";
    asset.lodCount  = 1;

    auto& lod = asset.lods[0];

    // Helper — push a quad (4 vertices, 6 indices).
    auto pushFace = [&](MeshVec3 p0, MeshVec3 p1, MeshVec3 p2, MeshVec3 p3,
                        MeshVec3 n, MeshVec3 t)
    {
        auto base = static_cast<std::uint32_t>(lod.vertices.size());

        auto makeVert = [&](MeshVec3 pos) -> MeshVertex
        {
            MeshVertex v{};
            v.position   = pos;
            v.normal     = n;
            v.tangent    = t;
            v.handedness = 1.0f;
            v.uv0        = { 0.0f, 0.0f };
            return v;
        };

        lod.vertices.push_back(makeVert(p0));
        lod.vertices.push_back(makeVert(p1));
        lod.vertices.push_back(makeVert(p2));
        lod.vertices.push_back(makeVert(p3));

        // Two CCW triangles: 0-1-2, 0-2-3
        lod.indices.push_back(base);
        lod.indices.push_back(base + 1);
        lod.indices.push_back(base + 2);
        lod.indices.push_back(base);
        lod.indices.push_back(base + 2);
        lod.indices.push_back(base + 3);
    };

    const float h = 0.5f;

    // +Z face (front)
    pushFace({ -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h },
             {  0,  0,  1 }, {  1,  0,  0 });

    // -Z face (back)
    pushFace({  h, -h, -h }, { -h, -h, -h }, { -h,  h, -h }, {  h,  h, -h },
             {  0,  0, -1 }, { -1,  0,  0 });

    // +X face (right)
    pushFace({  h, -h,  h }, {  h, -h, -h }, {  h,  h, -h }, {  h,  h,  h },
             {  1,  0,  0 }, {  0,  0, -1 });

    // -X face (left)
    pushFace({ -h, -h, -h }, { -h, -h,  h }, { -h,  h,  h }, { -h,  h, -h },
             { -1,  0,  0 }, {  0,  0,  1 });

    // +Y face (top)
    pushFace({ -h,  h,  h }, {  h,  h,  h }, {  h,  h, -h }, { -h,  h, -h },
             {  0,  1,  0 }, {  1,  0,  0 });

    // -Y face (bottom)
    pushFace({ -h, -h, -h }, {  h, -h, -h }, {  h, -h,  h }, { -h, -h,  h },
             {  0, -1,  0 }, {  1,  0,  0 });

    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint  = static_cast<std::uint32_t>(lod.indices.size());

    return asset;
}

} // anonymous namespace

// ─── IScenario lifecycle ─────────────────────────────────────────────────────

void RenderPlaygroundScenario::OnAttach(const ScenarioContext& ctx)
{
    m_backend      = ctx.renderBackend;
    m_requestClose = ctx.requestClose;
}

bool RenderPlaygroundScenario::OnInit()
{
    LOG_INFO(kTag, "Initialising render playground…");

    if (!m_backend)
    {
        LOG_ERROR(kTag, "No render backend — cannot run.");
        return false;
    }

    // ── Input ────────────────────────────────────────────────────────────────
    auto backend = std::make_unique<SagaEngine::Platform::SDLInputBackend>();
    m_inputManager.SetBackend(std::move(backend));
    if (!m_inputManager.Initialize())
    {
        LOG_ERROR(kTag, "InputManager init failed.");
        return false;
    }
    m_inputManager.RegisterDevice(Input::MakeKeyboardDevice(1));
    m_inputManager.RegisterDevice(Input::MakeMouseDevice(2));

    // Lock cursor for FPS-style mouse look.
    m_inputManager.SetCursorLocked(true);

    // ── Cube mesh ────────────────────────────────────────────────────────────
    auto cubeAsset = BuildCubeAsset();
    m_cubeMesh = m_backend->CreateMesh(cubeAsset);
    if (m_cubeMesh == World::MeshId::kInvalid)
    {
        LOG_ERROR(kTag, "Cube mesh upload failed.");
        return false;
    }

    // ── Solid material (no cull — debug visibility from all angles) ──────────
    MaterialRuntime mat{};
    mat.materialId  = 1;
    mat.renderQueue = MaterialRenderQueue::Opaque;
    mat.cullMode    = MaterialCullMode::Back;
    mat.writesDepth = true;

    m_cubeMaterial = m_backend->CreateMaterial(mat);
    if (m_cubeMaterial == World::MaterialId::kInvalid)
    {
        LOG_ERROR(kTag, "Cube material upload failed.");
        return false;
    }

    LOG_INFO(kTag, "Render playground ready (mesh=%u, mat=%u).",
             static_cast<unsigned>(m_cubeMesh),
             static_cast<unsigned>(m_cubeMaterial));
    return true;
}

void RenderPlaygroundScenario::OnUpdate(float dt, uint64_t tick)
{
    m_inputManager.Update(static_cast<uint32_t>(tick));

    const auto& state = m_inputManager.GetMergedState();
    const auto& kb    = state.keyboard;
    const auto& mouse = state.mouse;

    // ── ESC → close application ──────────────────────────────────────────────
    if (kb.WasJustPressed(Input::KeyCode::Escape) && m_requestClose)
        m_requestClose();

    // ── Mouse look ───────────────────────────────────────────────────────────
    constexpr float kMouseSensitivity = 0.003f;
    m_yaw   += static_cast<float>(mouse.relX) * kMouseSensitivity;
    m_pitch -= static_cast<float>(mouse.relY) * kMouseSensitivity;

    // Clamp pitch to avoid gimbal flip.
    constexpr float kMaxPitch = 1.5f;  // ~86°
    if (m_pitch >  kMaxPitch) m_pitch =  kMaxPitch;
    if (m_pitch < -kMaxPitch) m_pitch = -kMaxPitch;

    // ── Forward / right vectors from yaw + pitch ─────────────────────────────
    const float cy = std::cos(m_yaw);
    const float sy = std::sin(m_yaw);
    const float cp = std::cos(m_pitch);
    const float sp = std::sin(m_pitch);

    Math::Vec3 forward { sy * cp, sp, -cy * cp };  // RH: -Z is default forward
    Math::Vec3 right   { cy,      0.0f,  sy     };
    Math::Vec3 up      { 0.0f,    1.0f,  0.0f   };

    // ── WASD movement ────────────────────────────────────────────────────────
    constexpr float kMoveSpeed = 3.0f;
    float speed = kMoveSpeed * dt;

    // Shift sprint
    if (kb.IsDown(Input::KeyCode::LShift))
        speed *= 2.5f;

    if (kb.IsDown(Input::KeyCode::W)) { m_camPos.x += forward.x * speed; m_camPos.y += forward.y * speed; m_camPos.z += forward.z * speed; }
    if (kb.IsDown(Input::KeyCode::S)) { m_camPos.x -= forward.x * speed; m_camPos.y -= forward.y * speed; m_camPos.z -= forward.z * speed; }
    if (kb.IsDown(Input::KeyCode::D)) { m_camPos.x += right.x * speed;   m_camPos.y += right.y * speed;   m_camPos.z += right.z * speed;   }
    if (kb.IsDown(Input::KeyCode::A)) { m_camPos.x -= right.x * speed;   m_camPos.y -= right.y * speed;   m_camPos.z -= right.z * speed;   }
    if (kb.IsDown(Input::KeyCode::Space))  { m_camPos.y += speed; }
    if (kb.IsDown(Input::KeyCode::LCtrl))  { m_camPos.y -= speed; }

    // ── Auto-rotate the cube ─────────────────────────────────────────────────
    m_cubeAngle += dt * 0.8f;  // ~45 deg/sec
}

void RenderPlaygroundScenario::OnPrepareRender(
    SagaEngine::Render::Scene::Camera&     outCamera,
    SagaEngine::Render::Scene::RenderView& outView)
{
    // ── Camera ───────────────────────────────────────────────────────────────
    Math::Vec3 target {
        m_camPos.x + std::sin(m_yaw) * std::cos(m_pitch),
        m_camPos.y + std::sin(m_pitch),
        m_camPos.z - std::cos(m_yaw) * std::cos(m_pitch)
    };

    outCamera.position   = m_camPos;
    outCamera.view       = Math::Mat4::LookAtRH(m_camPos, target, { 0.0f, 1.0f, 0.0f });
    outCamera.projection = Math::Mat4::PerspectiveRH_ZO(
                               1.0472f,    // ~60° FOV
                               16.0f / 9.0f,  // default aspect; backend clears full backbuffer
                               0.1f,
                               200.0f);
    outCamera.RecomputeViewProj();

    // ── Cube draw item ───────────────────────────────────────────────────────
    SagaEngine::Render::Scene::DrawItem item{};
    item.mesh     = m_cubeMesh;
    item.material = m_cubeMaterial;
    item.lod      = 0;

    // Model matrix: rotate around Y axis.
    item.model = Math::Mat4::FromQuat(
        Math::Quat::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, m_cubeAngle));

    outView.drawItems.push_back(item);
}

void RenderPlaygroundScenario::OnShutdown()
{
    if (m_backend)
    {
        if (m_cubeMesh != World::MeshId::kInvalid)
            m_backend->DestroyMesh(m_cubeMesh);
        if (m_cubeMaterial != World::MaterialId::kInvalid)
            m_backend->DestroyMaterial(m_cubeMaterial);
    }

    m_cubeMesh     = World::MeshId::kInvalid;
    m_cubeMaterial = World::MaterialId::kInvalid;

    m_inputManager.SetCursorLocked(false);
    m_inputManager.Shutdown();
    LOG_INFO(kTag, "Render playground shut down.");
}

} // namespace SagaSandbox
