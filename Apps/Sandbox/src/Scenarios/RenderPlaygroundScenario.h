/// @file RenderPlaygroundScenario.h
/// @brief Interactive mini-scene with a rotating cube and fly-camera.
///
/// Layer  : Sandbox / Scenarios / Catalog
/// Purpose: Exercises the full Phase 3 render pipeline end-to-end:
///          CreateMesh (procedural cube), CreateMaterial (solid color),
///          Submit(RenderView) with per-object model matrix.
///          WASD + mouse fly-camera for interactive navigation.

#pragma once

#include "SagaSandbox/Core/IScenario.h"
#include <SagaEngine/Input/Core/InputManager.h>
#include <SagaEngine/Math/Mat4.h>
#include <SagaEngine/Math/Vec3.h>
#include <SagaEngine/Render/World/RenderEntity.h>

namespace SagaSandbox
{

class RenderPlaygroundScenario final : public IScenario
{
public:
    /// Static descriptor — required by SAGA_SANDBOX_REGISTER_SCENARIO.
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "render_playground",
        .displayName = "Render Playground",
        .category    = "Render",
        .description = "Rotating cube with WASD fly-camera — full Phase 3 pipeline exercise.",
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
    void OnShutdown()                               override;

private:
    // ── GPU resources ─────────────────────────────────────────────────────────

    ::SagaEngine::Render::Backend::IRenderBackend* m_backend = nullptr;

    ::SagaEngine::Render::World::MeshId     m_cubeMesh     = ::SagaEngine::Render::World::MeshId::kInvalid;
    ::SagaEngine::Render::World::MaterialId m_cubeMaterial = ::SagaEngine::Render::World::MaterialId::kInvalid;

    // ── Camera state ──────────────────────────────────────────────────────────

    ::SagaEngine::Math::Vec3 m_camPos   { 0.0f, 1.0f, 3.0f };
    float                    m_yaw      = 0.0f;   ///< Radians, around world Y.
    float                    m_pitch    = -0.2f;   ///< Radians, around local X.

    // ── Scene state ───────────────────────────────────────────────────────────

    float m_cubeAngle = 0.0f;   ///< Auto-rotation angle (radians).

    // ── Input ─────────────────────────────────────────────────────────────────

    SagaEngine::Input::InputManager m_inputManager;
};

} // namespace SagaSandbox
