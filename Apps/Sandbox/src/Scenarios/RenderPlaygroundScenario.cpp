/// @file RenderPlaygroundScenario.cpp
/// @brief Multi-object scene with frustum culling exercising the render pipeline.

#include "Scenarios/RenderPlaygroundScenario.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"

#include <SagaEngine/Animation/Skeleton.h>
#include <SagaEngine/Animation/AnimationClip.h>
#include <SagaEngine/Animation/PoseEvaluator.h>
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Input/Backends/SDL/SDLInputBackend.h>
#include <SagaEngine/Input/Devices/KeyboardDevice.h>
#include <SagaEngine/Input/Devices/MouseDevice.h>
#include <SagaEngine/Render/Materials/MeshAsset.h>
#include <SagaEngine/Render/Materials/Material.h>
#include <SagaEngine/Math/Quat.h>

#include <imgui.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(RenderPlaygroundScenario);

static constexpr const char* kTag = "RenderPlayground";

namespace Input = SagaEngine::Input;
namespace Math  = SagaEngine::Math;
namespace RB    = SagaEngine::Render::Backend;
namespace World = SagaEngine::Render::World;
namespace Scene = SagaEngine::Render::Scene;

using SagaEngine::Render::MeshAsset;
using SagaEngine::Render::MeshVertex;
using SagaEngine::Render::MeshVec3;
using SagaEngine::Render::MeshVec2;
using SagaEngine::Render::MaterialRuntime;
using SagaEngine::Render::MaterialRenderQueue;
using SagaEngine::Render::MaterialCullMode;
using SagaEngine::Render::MaterialTextureSlot;
using SagaEngine::Render::TextureHandle;

namespace Anim = SagaEngine::Animation;

// ─── Procedural mesh builders ───────────────────────────────────────────────

namespace
{

/// Build a procedural cylinder with 3 bones along Y axis.
/// Segments = rings along the height; slices = verts around the circumference.
/// Bone 0: Y [0, segHeight), Bone 1: Y [segHeight, 2*segHeight), Bone 2: top.
/// Each vertex is assigned to 1-2 bones with smooth weight blending near joints.
MeshAsset BuildSkinnedCylinderAsset(float radius, float height,
                                     int slices, int rings)
{
    MeshAsset asset{};
    asset.meshId    = 3;
    asset.debugName = "SkinnedCylinder";
    asset.lodCount  = 1;

    auto& lod = asset.lods[0];

    const float segHeight = height / 3.0f;  // 3 bones split the cylinder
    constexpr float kPi = 3.14159265358979323846f;
    const float pi2 = 2.0f * kPi;

    // Generate rings+1 rows of slices+1 verts.
    for (int iy = 0; iy <= rings; ++iy)
    {
        const float t = static_cast<float>(iy) / static_cast<float>(rings);
        const float y = t * height;

        for (int ix = 0; ix <= slices; ++ix)
        {
            const float u = static_cast<float>(ix) / static_cast<float>(slices);
            const float angle = u * pi2;
            const float cx = std::cos(angle);
            const float cz = std::sin(angle);

            MeshVertex v{};
            v.position = { radius * cx, y, radius * cz };
            v.normal   = { cx, 0.0f, cz };
            v.tangent  = { -cz, 0.0f, cx };
            v.handedness = 1.0f;
            v.uv0 = { u, t };

            // Bone assignment: linear blend in transition zones.
            // boneIndex = which third of the cylinder this y falls in.
            const float boneF = y / segHeight;         // 0..3
            const int   boneBase = std::min(static_cast<int>(boneF), 2);

            // Blend zone: within 20% of the segment boundary.
            const float localT = boneF - static_cast<float>(boneBase);
            const float blendZone = 0.2f;

            if (boneBase < 2 && localT > (1.0f - blendZone))
            {
                // Near the top of this segment — blend with next bone.
                const float blend = (localT - (1.0f - blendZone)) / blendZone;
                v.boneIndices[0] = static_cast<std::uint8_t>(boneBase);
                v.boneIndices[1] = static_cast<std::uint8_t>(boneBase + 1);
                v.boneWeights[0] = 1.0f - blend;
                v.boneWeights[1] = blend;
            }
            else if (boneBase > 0 && localT < blendZone)
            {
                // Near the bottom of this segment — blend with previous bone.
                const float blend = localT / blendZone;
                v.boneIndices[0] = static_cast<std::uint8_t>(boneBase - 1);
                v.boneIndices[1] = static_cast<std::uint8_t>(boneBase);
                v.boneWeights[0] = 1.0f - blend;
                v.boneWeights[1] = blend;
            }
            else
            {
                // Fully owned by one bone.
                v.boneIndices[0] = static_cast<std::uint8_t>(boneBase);
                v.boneWeights[0] = 1.0f;
            }

            lod.vertices.push_back(v);
        }
    }

    // Triangulate.
    const int vertsPerRow = slices + 1;
    for (int iy = 0; iy < rings; ++iy)
    {
        for (int ix = 0; ix < slices; ++ix)
        {
            const auto a = static_cast<std::uint32_t>(iy * vertsPerRow + ix);
            const auto b = a + 1;
            const auto c = static_cast<std::uint32_t>((iy + 1) * vertsPerRow + ix);
            const auto d = c + 1;

            lod.indices.push_back(a);
            lod.indices.push_back(c);
            lod.indices.push_back(b);

            lod.indices.push_back(b);
            lod.indices.push_back(c);
            lod.indices.push_back(d);
        }
    }

    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint  = static_cast<std::uint32_t>(lod.indices.size());

    return asset;
}

/// Build a 3-bone skeleton for the cylinder (joints along Y axis).
Anim::Skeleton BuildArmSkeleton(float height)
{
    const float seg = height / 3.0f;

    Anim::Skeleton skel;
    skel.name = "ArmSkeleton";

    // Bone 0: root at Y=0.
    Anim::Joint j0;
    j0.name         = "Root";
    j0.parent       = Anim::kNoParent;
    j0.restPosition = { 0.0f, 0.0f, 0.0f };
    skel.joints.push_back(j0);

    // Bone 1: at Y = seg (child of root).
    Anim::Joint j1;
    j1.name         = "Mid";
    j1.parent       = 0;
    j1.restPosition = { 0.0f, seg, 0.0f };
    skel.joints.push_back(j1);

    // Bone 2: at Y = 2*seg (child of mid).
    Anim::Joint j2;
    j2.name         = "Tip";
    j2.parent       = 1;
    j2.restPosition = { 0.0f, seg, 0.0f };
    skel.joints.push_back(j2);

    // Compute inverse bind matrices from the rest pose.
    Math::Mat4 worldPoses[3];
    skel.ComputeRestPose(worldPoses);
    for (int i = 0; i < 3; ++i)
        skel.joints[static_cast<std::size_t>(i)].inverseBindMatrix = worldPoses[i].Inverse();

    return skel;
}

/// Build a waving animation clip: bone 1 and 2 rotate around Z axis.
Anim::AnimationClip BuildWaveClip(float duration)
{
    Anim::AnimationClip clip;
    clip.name     = "Wave";
    clip.duration = duration;
    clip.looping  = true;

    const float maxAngle = 0.5f;  // ~28 degrees

    // Bone 1 (Mid) rotation around Z.
    {
        Anim::AnimationChannel ch;
        ch.jointIndex = 1;
        ch.target     = Anim::ChannelTarget::Rotation;

        // Keyframes: identity → tilt → identity → tilt other way → identity
        const float q = duration / 4.0f;
        ch.quatKeys.push_back({ 0.0f,     Math::Quat::Identity() });
        ch.quatKeys.push_back({ q,        Math::Quat::FromAxisAngle({ 0.0f, 0.0f, 1.0f },  maxAngle) });
        ch.quatKeys.push_back({ 2.0f * q, Math::Quat::Identity() });
        ch.quatKeys.push_back({ 3.0f * q, Math::Quat::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, -maxAngle) });
        ch.quatKeys.push_back({ duration,  Math::Quat::Identity() });

        clip.channels.push_back(std::move(ch));
    }

    // Bone 2 (Tip) — slightly more dramatic rotation.
    {
        Anim::AnimationChannel ch;
        ch.jointIndex = 2;
        ch.target     = Anim::ChannelTarget::Rotation;

        const float bigAngle = maxAngle * 1.5f;
        const float q = duration / 4.0f;
        ch.quatKeys.push_back({ 0.0f,     Math::Quat::Identity() });
        ch.quatKeys.push_back({ q,        Math::Quat::FromAxisAngle({ 0.0f, 0.0f, 1.0f },  bigAngle) });
        ch.quatKeys.push_back({ 2.0f * q, Math::Quat::Identity() });
        ch.quatKeys.push_back({ 3.0f * q, Math::Quat::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, -bigAngle) });
        ch.quatKeys.push_back({ duration,  Math::Quat::Identity() });

        clip.channels.push_back(std::move(ch));
    }

    return clip;
}

/// Build a unit cube centred at origin with per-face normals and UVs.
MeshAsset BuildCubeAsset()
{
    MeshAsset asset{};
    asset.meshId    = 1;
    asset.debugName = "ProceduralCube";
    asset.lodCount  = 1;

    auto& lod = asset.lods[0];

    auto pushFace = [&](MeshVec3 p0, MeshVec3 p1, MeshVec3 p2, MeshVec3 p3,
                        MeshVec3 n, MeshVec3 t)
    {
        auto base = static_cast<std::uint32_t>(lod.vertices.size());

        auto makeVert = [&](MeshVec3 pos, MeshVec2 uv) -> MeshVertex
        {
            MeshVertex v{};
            v.position   = pos;
            v.normal     = n;
            v.tangent    = t;
            v.handedness = 1.0f;
            v.uv0        = uv;
            return v;
        };

        lod.vertices.push_back(makeVert(p0, { 0.0f, 1.0f }));
        lod.vertices.push_back(makeVert(p1, { 1.0f, 1.0f }));
        lod.vertices.push_back(makeVert(p2, { 1.0f, 0.0f }));
        lod.vertices.push_back(makeVert(p3, { 0.0f, 0.0f }));

        lod.indices.push_back(base);
        lod.indices.push_back(base + 1);
        lod.indices.push_back(base + 2);
        lod.indices.push_back(base);
        lod.indices.push_back(base + 2);
        lod.indices.push_back(base + 3);
    };

    const float h = 0.5f;

    pushFace({ -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h },
             {  0,  0,  1 }, {  1,  0,  0 });
    pushFace({  h, -h, -h }, { -h, -h, -h }, { -h,  h, -h }, {  h,  h, -h },
             {  0,  0, -1 }, { -1,  0,  0 });
    pushFace({  h, -h,  h }, {  h, -h, -h }, {  h,  h, -h }, {  h,  h,  h },
             {  1,  0,  0 }, {  0,  0, -1 });
    pushFace({ -h, -h, -h }, { -h, -h,  h }, { -h,  h,  h }, { -h,  h, -h },
             { -1,  0,  0 }, {  0,  0,  1 });
    pushFace({ -h,  h,  h }, {  h,  h,  h }, {  h,  h, -h }, { -h,  h, -h },
             {  0,  1,  0 }, {  1,  0,  0 });
    pushFace({ -h, -h, -h }, {  h, -h, -h }, {  h, -h,  h }, { -h, -h,  h },
             {  0, -1,  0 }, {  1,  0,  0 });

    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint  = static_cast<std::uint32_t>(lod.indices.size());

    return asset;
}

/// Build a flat ground plane centred at origin (XZ plane, Y=0).
MeshAsset BuildGroundPlaneAsset(float halfExtent, float uvScale)
{
    MeshAsset asset{};
    asset.meshId    = 2;
    asset.debugName = "GroundPlane";
    asset.lodCount  = 1;

    auto& lod = asset.lods[0];

    const float h = halfExtent;

    MeshVertex v0{}, v1{}, v2{}, v3{};

    v0.position = { -h, 0.0f,  h };
    v1.position = {  h, 0.0f,  h };
    v2.position = {  h, 0.0f, -h };
    v3.position = { -h, 0.0f, -h };

    MeshVec3 n = { 0.0f, 1.0f, 0.0f };
    MeshVec3 t = { 1.0f, 0.0f, 0.0f };

    v0.normal = v1.normal = v2.normal = v3.normal = n;
    v0.tangent = v1.tangent = v2.tangent = v3.tangent = t;
    v0.handedness = v1.handedness = v2.handedness = v3.handedness = 1.0f;

    v0.uv0 = { 0.0f,     uvScale };
    v1.uv0 = { uvScale,  uvScale };
    v2.uv0 = { uvScale,  0.0f    };
    v3.uv0 = { 0.0f,     0.0f    };

    lod.vertices = { v0, v1, v2, v3 };
    lod.indices  = { 0, 1, 2, 0, 2, 3 };

    lod.vertexCountHint = 4;
    lod.indexCountHint  = 6;

    return asset;
}

/// Simple pseudo-random float in [0, 1] from a seed.
float HashFloat(uint32_t seed)
{
    seed ^= seed >> 16;
    seed *= 0x45d9f3bu;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0xFFFFu) / 65535.0f;
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
    m_inputManager.SetCursorLocked(true);

    // ── Cube mesh ────────────────────────────────────────────────────────────
    auto cubeAsset = BuildCubeAsset();
    m_cubeMesh = m_backend->CreateMesh(cubeAsset);
    if (m_cubeMesh == World::MeshId::kInvalid)
    {
        LOG_ERROR(kTag, "Cube mesh upload failed.");
        return false;
    }

    // ── Ground plane mesh ────────────────────────────────────────────────────
    auto groundAsset = BuildGroundPlaneAsset(50.0f, 25.0f);
    m_groundMesh = m_backend->CreateMesh(groundAsset);
    if (m_groundMesh == World::MeshId::kInvalid)
        LOG_WARN(kTag, "Ground plane mesh upload failed.");

    // ── Checkerboard texture (cubes) ─────────────────────────────────────────
    {
        constexpr uint32_t kTexSize = 64;
        constexpr uint32_t kCellSize = 8;
        std::vector<uint8_t> pixels(kTexSize * kTexSize * 4);

        for (uint32_t y = 0; y < kTexSize; ++y)
        {
            for (uint32_t x = 0; x < kTexSize; ++x)
            {
                const bool white = ((x / kCellSize) + (y / kCellSize)) % 2 == 0;
                const uint8_t c = white ? 230 : 50;
                const uint32_t idx = (y * kTexSize + x) * 4;
                pixels[idx + 0] = c;
                pixels[idx + 1] = c;
                pixels[idx + 2] = c;
                pixels[idx + 3] = 255;
            }
        }
        m_checkerTex = m_backend->CreateTexture(kTexSize, kTexSize, pixels.data());
    }

    // ── Ground texture (grey grid pattern) ───────────────────────────────────
    {
        constexpr uint32_t kTexSize = 64;
        std::vector<uint8_t> pixels(kTexSize * kTexSize * 4);

        for (uint32_t y = 0; y < kTexSize; ++y)
        {
            for (uint32_t x = 0; x < kTexSize; ++x)
            {
                const bool edge = (x % 32 < 2) || (y % 32 < 2);
                const uint8_t c = edge ? 100 : 60;
                const uint32_t idx = (y * kTexSize + x) * 4;
                pixels[idx + 0] = c;
                pixels[idx + 1] = c + 5;
                pixels[idx + 2] = c + 10;
                pixels[idx + 3] = 255;
            }
        }
        m_groundTex = m_backend->CreateTexture(kTexSize, kTexSize, pixels.data());
    }

    // ── Cube material ────────────────────────────────────────────────────────
    {
        MaterialRuntime mat{};
        mat.materialId  = 1;
        mat.renderQueue = MaterialRenderQueue::Opaque;
        mat.cullMode    = MaterialCullMode::Back;
        mat.writesDepth = true;
        mat.textures[static_cast<std::size_t>(MaterialTextureSlot::Albedo)] = m_checkerTex;

        m_cubeMaterial = m_backend->CreateMaterial(mat);
        if (m_cubeMaterial == World::MaterialId::kInvalid)
        {
            LOG_ERROR(kTag, "Cube material upload failed.");
            return false;
        }
    }

    // ── Ground material ──────────────────────────────────────────────────────
    if (m_groundMesh != World::MeshId::kInvalid)
    {
        MaterialRuntime mat{};
        mat.materialId  = 2;
        mat.renderQueue = MaterialRenderQueue::Opaque;
        mat.cullMode    = MaterialCullMode::Back;
        mat.writesDepth = true;
        mat.textures[static_cast<std::size_t>(MaterialTextureSlot::Albedo)] = m_groundTex;

        m_groundMat = m_backend->CreateMaterial(mat);
    }

    // ── Skinned cylinder mesh ──────────────────────────────────────────────────
    {
        constexpr float kCylRadius = 0.3f;
        constexpr float kCylHeight = 3.0f;
        auto cylAsset = BuildSkinnedCylinderAsset(kCylRadius, kCylHeight, 16, 24);
        m_armMesh = m_backend->CreateMesh(cylAsset);

        if (m_armMesh == World::MeshId::kInvalid)
            LOG_WARN(kTag, "Skinned cylinder mesh upload failed.");
    }

    // ── Arm texture (gradient from blue at bottom to orange at top) ──────────
    {
        constexpr uint32_t kTexSize = 64;
        std::vector<uint8_t> pixels(kTexSize * kTexSize * 4);

        for (uint32_t y = 0; y < kTexSize; ++y)
        {
            const float t = static_cast<float>(y) / static_cast<float>(kTexSize - 1);
            const uint8_t r = static_cast<uint8_t>(60  + t * 195.0f);
            const uint8_t g = static_cast<uint8_t>(100 + t * 80.0f);
            const uint8_t b = static_cast<uint8_t>(200 - t * 140.0f);

            for (uint32_t x = 0; x < kTexSize; ++x)
            {
                const uint32_t idx = (y * kTexSize + x) * 4;
                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = 255;
            }
        }
        m_armTex = m_backend->CreateTexture(kTexSize, kTexSize, pixels.data());
    }

    // ── Arm material ────────────────────────────────────────────────────────
    if (m_armMesh != World::MeshId::kInvalid)
    {
        MaterialRuntime mat{};
        mat.materialId  = 3;
        mat.renderQueue = MaterialRenderQueue::Opaque;
        mat.cullMode    = MaterialCullMode::Back;
        mat.writesDepth = true;
        mat.textures[static_cast<std::size_t>(MaterialTextureSlot::Albedo)] = m_armTex;

        m_armMaterial = m_backend->CreateMaterial(mat);
    }

    // ── Skeleton + animation ────────────────────────────────────────────────
    {
        constexpr float kCylHeight = 3.0f;
        m_armSkeleton = BuildArmSkeleton(kCylHeight);
        m_armClip     = BuildWaveClip(2.0f);  // 2-second loop

        m_armAnimState.clip    = &m_armClip;
        m_armAnimState.time    = 0.0f;
        m_armAnimState.speed   = 1.0f;
        m_armAnimState.playing = true;

        m_armPoseBuffer.Resize(m_armSkeleton.JointCount());
    }

    // ── Populate scene objects: 10x10 grid of cubes ──────────────────────────
    constexpr int   kGridSize = 10;
    constexpr float kSpacing  = 3.0f;
    constexpr float kOffset   = (kGridSize - 1) * kSpacing * 0.5f;

    m_objects.reserve(kGridSize * kGridSize);

    for (int gz = 0; gz < kGridSize; ++gz)
    {
        for (int gx = 0; gx < kGridSize; ++gx)
        {
            const uint32_t seed = static_cast<uint32_t>(gz * kGridSize + gx + 42);

            SceneObject obj{};
            obj.position.x = static_cast<float>(gx) * kSpacing - kOffset;
            obj.position.y = 0.5f + HashFloat(seed + 100) * 1.5f;  // Y: 0.5 .. 2.0
            obj.position.z = static_cast<float>(gz) * kSpacing - kOffset;

            // Random rotation axis (normalised-ish).
            obj.rotAxis.x = HashFloat(seed + 200) * 2.0f - 1.0f;
            obj.rotAxis.y = HashFloat(seed + 300) * 2.0f - 1.0f + 0.5f;
            obj.rotAxis.z = HashFloat(seed + 400) * 2.0f - 1.0f;
            const float axLen = std::sqrt(obj.rotAxis.x * obj.rotAxis.x
                                        + obj.rotAxis.y * obj.rotAxis.y
                                        + obj.rotAxis.z * obj.rotAxis.z);
            if (axLen > 0.001f)
            {
                obj.rotAxis.x /= axLen;
                obj.rotAxis.y /= axLen;
                obj.rotAxis.z /= axLen;
            }

            obj.rotSpeed = 0.3f + HashFloat(seed + 500) * 1.5f;  // 0.3 .. 1.8 rad/s
            obj.scale    = 0.4f + HashFloat(seed + 600) * 0.8f;  // 0.4 .. 1.2

            // Bounding sphere: unit cube inscribed sphere * scale.
            obj.boundsRadius = obj.scale * 0.87f;

            obj.angle = HashFloat(seed + 700) * 6.28f;  // Random start angle.

            m_objects.push_back(obj);
        }
    }

    LOG_INFO(kTag, "Render playground ready: %zu scene objects.", m_objects.size());
    return true;
}

void RenderPlaygroundScenario::OnUpdate(float dt, uint64_t tick)
{
    m_inputManager.Update(static_cast<uint32_t>(tick));

    const auto& state = m_inputManager.GetMergedState();
    const auto& kb    = state.keyboard;
    const auto& mouse = state.mouse;

    if (kb.WasJustPressed(Input::KeyCode::Escape) && m_requestClose)
        m_requestClose();

    if (kb.WasJustPressed(Input::KeyCode::Tab))
    {
        m_cursorLocked = !m_cursorLocked;
        m_inputManager.SetCursorLocked(m_cursorLocked);
    }

    if (m_cursorLocked)
    {
        constexpr float kMouseSensitivity = 0.003f;
        m_yaw   += static_cast<float>(mouse.relX) * kMouseSensitivity;
        m_pitch -= static_cast<float>(mouse.relY) * kMouseSensitivity;

        constexpr float kMaxPitch = 1.5f;
        if (m_pitch >  kMaxPitch) m_pitch =  kMaxPitch;
        if (m_pitch < -kMaxPitch) m_pitch = -kMaxPitch;
    }

    const float cy = std::cos(m_yaw);
    const float sy = std::sin(m_yaw);
    const float cp = std::cos(m_pitch);
    const float sp = std::sin(m_pitch);

    Math::Vec3 forward { sy * cp, sp, -cy * cp };
    Math::Vec3 right   { cy,      0.0f,  sy     };

    if (m_cursorLocked)
    {
        constexpr float kMoveSpeed = 5.0f;
        float speed = kMoveSpeed * dt;

        if (kb.IsDown(Input::KeyCode::LShift))
            speed *= 2.5f;

        if (kb.IsDown(Input::KeyCode::W)) { m_camPos.x += forward.x * speed; m_camPos.y += forward.y * speed; m_camPos.z += forward.z * speed; }
        if (kb.IsDown(Input::KeyCode::S)) { m_camPos.x -= forward.x * speed; m_camPos.y -= forward.y * speed; m_camPos.z -= forward.z * speed; }
        if (kb.IsDown(Input::KeyCode::D)) { m_camPos.x += right.x * speed;   m_camPos.y += right.y * speed;   m_camPos.z += right.z * speed;   }
        if (kb.IsDown(Input::KeyCode::A)) { m_camPos.x -= right.x * speed;   m_camPos.y -= right.y * speed;   m_camPos.z -= right.z * speed;   }
        if (kb.IsDown(Input::KeyCode::Space))  { m_camPos.y += speed; }
        if (kb.IsDown(Input::KeyCode::LCtrl))  { m_camPos.y -= speed; }
    }

    // ── Auto-rotate all objects ──────────────────────────────────────────────
    for (auto& obj : m_objects)
        obj.angle += dt * obj.rotSpeed;

    // ── Tick skeletal animation ──────────────────────────────────────────────
    m_armAnimState.Tick(dt);
    Anim::EvaluatePose(m_armSkeleton, m_armAnimState, m_armPoseBuffer);
}

void RenderPlaygroundScenario::OnPrepareRender(
    Scene::Camera&     outCamera,
    Scene::RenderView& outView)
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
                               1.0472f,        // ~60 FOV
                               16.0f / 9.0f,
                               0.1f,
                               200.0f);
    outCamera.RecomputeViewProj();
    outCamera.RecomputeFrustum();

    const auto& frustum = outCamera.frustum;

    // ── Ground plane (always visible — large quad, skip cull) ────────────────
    if (m_groundMesh != World::MeshId::kInvalid &&
        m_groundMat  != World::MaterialId::kInvalid)
    {
        Scene::DrawItem ground{};
        ground.mesh     = m_groundMesh;
        ground.material = m_groundMat;
        ground.model    = Math::Mat4::Identity();
        outView.drawItems.push_back(ground);
    }

    // ── Skinned cylinder draw item ─────────────────────────────────────────────
    if (m_armMesh != World::MeshId::kInvalid &&
        m_armMaterial != World::MaterialId::kInvalid &&
        m_armPoseBuffer.JointCount() > 0)
    {
        Scene::DrawItem arm{};
        arm.mesh     = m_armMesh;
        arm.material = m_armMaterial;
        // Place the arm at (-3, 0, 0) so it's clearly visible next to the cubes.
        arm.model    = Math::Mat4::FromTranslation({ -3.0f, 0.0f, 0.0f });

        // Attach bone palette — pointer to flat float array of skin matrices.
        arm.boneMatrices = m_armPoseBuffer.skinMatrices[0].Data();
        arm.boneCount    = m_armPoseBuffer.JointCount();

        outView.drawItems.push_back(arm);
    }

    // ── Frustum-cull scene objects and emit draw items ────────────────────────
    uint32_t total   = static_cast<uint32_t>(m_objects.size());
    uint32_t visible = 0;

    for (const auto& obj : m_objects)
    {
        // Sphere-vs-frustum test.
        if (!frustum.IntersectsSphere(obj.position, obj.boundsRadius))
            continue;

        ++visible;

        Scene::DrawItem item{};
        item.mesh     = m_cubeMesh;
        item.material = m_cubeMaterial;

        // Model = Translation * Rotation * Scale.
        Math::Mat4 rot   = Math::Mat4::FromQuat(
            Math::Quat::FromAxisAngle(obj.rotAxis, obj.angle));
        Math::Mat4 trans = Math::Mat4::FromTranslation(obj.position);
        Math::Mat4 scale = Math::Mat4::FromScale({ obj.scale, obj.scale, obj.scale });

        item.model = trans * rot * scale;

        outView.drawItems.push_back(item);
    }

    // ── Update stats for debug HUD ───────────────────────────────────────────
    m_totalObjects   = total;
    m_visibleObjects = visible;
    m_culledObjects  = total - visible;
    m_drawCalls      = static_cast<uint32_t>(outView.drawItems.size());
}

void RenderPlaygroundScenario::OnRenderDebugUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 130), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Render Stats"))
    {
        ImGui::Text("Objects: %u total", m_totalObjects);
        ImGui::Text("Visible: %u", m_visibleObjects);
        ImGui::Text("Culled:  %u", m_culledObjects);
        ImGui::Text("Draw calls: %u", m_drawCalls);
        ImGui::Separator();
        ImGui::Text("Cam: %.1f, %.1f, %.1f",
                     m_camPos.x, m_camPos.y, m_camPos.z);

        if (m_armAnimState.clip)
        {
            ImGui::Separator();
            ImGui::Text("Skeleton: %u bones", m_armSkeleton.JointCount());
            ImGui::Text("Anim: %.2f / %.2fs  %s",
                         m_armAnimState.time,
                         m_armAnimState.clip->duration,
                         m_armAnimState.playing ? "playing" : "stopped");
        }

        ImGui::Separator();
        ImGui::Text("[Tab] toggle cursor | [ESC] quit");
    }
    ImGui::End();
}

void RenderPlaygroundScenario::OnShutdown()
{
    if (m_backend)
    {
        if (m_cubeMesh != World::MeshId::kInvalid)
            m_backend->DestroyMesh(m_cubeMesh);
        if (m_cubeMaterial != World::MaterialId::kInvalid)
            m_backend->DestroyMaterial(m_cubeMaterial);
        if (m_groundMesh != World::MeshId::kInvalid)
            m_backend->DestroyMesh(m_groundMesh);
        if (m_groundMat != World::MaterialId::kInvalid)
            m_backend->DestroyMaterial(m_groundMat);
        if (m_checkerTex != TextureHandle::kInvalid)
            m_backend->DestroyTexture(m_checkerTex);
        if (m_groundTex != TextureHandle::kInvalid)
            m_backend->DestroyTexture(m_groundTex);
        if (m_armMesh != World::MeshId::kInvalid)
            m_backend->DestroyMesh(m_armMesh);
        if (m_armMaterial != World::MaterialId::kInvalid)
            m_backend->DestroyMaterial(m_armMaterial);
        if (m_armTex != TextureHandle::kInvalid)
            m_backend->DestroyTexture(m_armTex);
    }

    m_cubeMesh     = World::MeshId::kInvalid;
    m_cubeMaterial = World::MaterialId::kInvalid;
    m_groundMesh   = World::MeshId::kInvalid;
    m_groundMat    = World::MaterialId::kInvalid;
    m_armMesh      = World::MeshId::kInvalid;
    m_armMaterial  = World::MaterialId::kInvalid;
    m_checkerTex   = TextureHandle::kInvalid;
    m_groundTex    = TextureHandle::kInvalid;
    m_armTex       = TextureHandle::kInvalid;
    m_objects.clear();

    m_inputManager.SetCursorLocked(false);
    m_inputManager.Shutdown();
    LOG_INFO(kTag, "Render playground shut down.");
}

} // namespace SagaSandbox
