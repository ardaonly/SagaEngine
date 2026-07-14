/// @file RenderPlaygroundScenario.h
/// @brief Multi-object scene with frustum culling and WASD fly-camera.
///
/// Layer  : Sandbox / Scenarios / Catalog
/// Purpose: Exercises the full Phase 3+ render pipeline end-to-end:
///          100+ objects, frustum culling, textured + lit materials,
///          per-object transforms, ImGui debug stats overlay.

#pragma once

#include "SagaSandbox/Core/IScenario.h"
#include <SagaEngine/Animation/Skeleton.h>
#include <SagaEngine/Animation/AnimationClip.h>
#include <SagaEngine/Animation/PoseEvaluator.h>
#include <SagaEngine/Input/Core/InputManager.h>
#include <SagaEngine/Math/Mat4.h>
#include <SagaEngine/Math/Vec3.h>
#include <SagaEngine/Render/Materials/Material.h>
#include <SagaEngine/Render/World/RenderEntity.h>

#include <functional>
#include <memory>
#include <vector>

namespace SagaSandbox
{

class RenderPlaygroundScenario final : public IScenario
{
public:
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "render_playground",
        .displayName = "Render Playground",
        .category    = "Render",
        .description = "Multi-object scene with frustum culling, WASD fly-camera, and debug stats.",
    };

    RenderPlaygroundScenario()  = default;
    ~RenderPlaygroundScenario() override = default;

    // ── IScenario ─────────────────────────────────────────────────────────────

    [[nodiscard]] const ScenarioDescriptor& GetDescriptor() const noexcept override
    {
        return kDescriptor;
    }

    void OnAttach(const ScenarioContext& ctx)       override;
    [[nodiscard]] bool OnInit()                     override;
    void OnUpdate(float dt, uint64_t tick)          override;
    void OnPrepareRender(
        ::SagaEngine::Render::Scene::Camera&     outCamera,
        ::SagaEngine::Render::Scene::RenderView& outView) override;
    void OnRenderDebugUI()                          override;
    void OnShutdown()                               override;

private:
    // ── Scene object descriptor ───────────────────────────────────────────────

    struct SceneObject
    {
        ::SagaEngine::Math::Vec3 position{};
        ::SagaEngine::Math::Vec3 rotAxis{ 0.0f, 1.0f, 0.0f };
        float rotSpeed     = 0.0f;       ///< Radians/sec auto-rotation.
        float scale        = 1.0f;
        float boundsRadius = 0.87f;      ///< Bounding sphere radius (unit cube diagonal/2).
        float angle        = 0.0f;       ///< Current rotation angle.
    };

    // ── GPU resources ─────────────────────────────────────────────────────────

    ::SagaEngine::Render::Backend::IRenderBackend* m_backend = nullptr;

    ::SagaEngine::Render::World::MeshId     m_cubeMesh     = ::SagaEngine::Render::World::MeshId::kInvalid;
    ::SagaEngine::Render::World::MaterialId m_cubeMaterial = ::SagaEngine::Render::World::MaterialId::kInvalid;
    ::SagaEngine::Render::World::MeshId     m_groundMesh   = ::SagaEngine::Render::World::MeshId::kInvalid;
    ::SagaEngine::Render::World::MaterialId m_groundMat    = ::SagaEngine::Render::World::MaterialId::kInvalid;
    ::SagaEngine::Render::TextureHandle     m_checkerTex   = ::SagaEngine::Render::TextureHandle::kInvalid;
    ::SagaEngine::Render::TextureHandle     m_groundTex    = ::SagaEngine::Render::TextureHandle::kInvalid;

    // ── Scene objects ─────────────────────────────────────────────────────────

    std::vector<SceneObject> m_objects;

    // ── Camera state ──────────────────────────────────────────────────────────

    ::SagaEngine::Math::Vec3 m_camPos   { 0.0f, 5.0f, 15.0f };
    float                    m_yaw      = 0.0f;
    float                    m_pitch    = -0.3f;

    // ── Culling stats (updated per frame) ─────────────────────────────────────

    uint32_t m_totalObjects  = 0;
    uint32_t m_visibleObjects = 0;
    uint32_t m_culledObjects  = 0;
    uint32_t m_drawCalls      = 0;

    // ── Skinned cylinder demo ────────────────────────────────────────────────

    ::SagaEngine::Render::World::MeshId     m_armMesh     = ::SagaEngine::Render::World::MeshId::kInvalid;
    ::SagaEngine::Render::World::MaterialId m_armMaterial = ::SagaEngine::Render::World::MaterialId::kInvalid;
    ::SagaEngine::Render::TextureHandle     m_armTex      = ::SagaEngine::Render::TextureHandle::kInvalid;

    ::SagaEngine::Animation::Skeleton       m_armSkeleton;
    ::SagaEngine::Animation::AnimationClip  m_armClip;
    ::SagaEngine::Animation::AnimationState m_armAnimState;
    ::SagaEngine::Animation::PoseBuffer     m_armPoseBuffer;

    // ── Input ─────────────────────────────────────────────────────────────────

    SagaEngine::Input::InputManager m_inputManager;
    bool m_cursorLocked = true;

    // ── Host callback ─────────────────────────────────────────────────────────

    std::function<void()> m_requestClose;
};

} // namespace SagaSandbox
