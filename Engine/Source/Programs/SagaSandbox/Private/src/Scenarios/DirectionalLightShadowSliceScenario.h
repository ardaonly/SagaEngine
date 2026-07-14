/// @file DirectionalLightShadowSliceScenario.h
/// @brief Playable directional-light and shadow-map render slice.

#pragma once

#include "SagaSandbox/Core/IScenario.h"

#include <SagaEngine/Input/Core/InputManager.h>
#include <SagaEngine/Math/Vec3.h>
#include <SagaEngine/Render/Materials/Material.h>

#include <functional>
#include <vector>

namespace SagaSandbox
{

class DirectionalLightShadowSliceScenario final : public IScenario
{
public:
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "directional_light_shadow_slice",
        .displayName = "Directional Light Shadow Slice",
        .category    = "Render",
        .description = "Lit cubes, directional shadows, shadow toggle, light rotation, and fly-camera controls.",
    };

    [[nodiscard]] const ScenarioDescriptor& GetDescriptor() const noexcept override
    {
        return kDescriptor;
    }

    void OnAttach(const ScenarioContext& ctx) override;
    [[nodiscard]] bool OnInit() override;
    void OnUpdate(float dt, uint64_t tick) override;
    void OnPrepareRender(
        ::SagaEngine::Render::Scene::Camera& outCamera,
        ::SagaEngine::Render::Scene::RenderView& outView) override;
    void OnRenderDebugUI() override;
    void OnShutdown() override;

private:
    struct CubeInstance
    {
        ::SagaEngine::Math::Vec3 position{};
        ::SagaEngine::Math::Vec3 scale{1.0f, 1.0f, 1.0f};
        ::SagaEngine::Render::World::MaterialId material =
            ::SagaEngine::Render::World::MaterialId::kInvalid;
        float rotation = 0.0f;
        float rotationSpeed = 0.0f;
        bool moving = false;
    };

    ::SagaEngine::Render::Backend::IRenderBackend* m_backend = nullptr;
    std::function<void()> m_requestClose;

    ::SagaEngine::Render::World::MeshId m_cubeMesh =
        ::SagaEngine::Render::World::MeshId::kInvalid;
    ::SagaEngine::Render::World::MeshId m_groundMesh =
        ::SagaEngine::Render::World::MeshId::kInvalid;

    ::SagaEngine::Render::World::MaterialId m_cubeMaterialA =
        ::SagaEngine::Render::World::MaterialId::kInvalid;
    ::SagaEngine::Render::World::MaterialId m_cubeMaterialB =
        ::SagaEngine::Render::World::MaterialId::kInvalid;
    ::SagaEngine::Render::World::MaterialId m_groundMaterial =
        ::SagaEngine::Render::World::MaterialId::kInvalid;

    ::SagaEngine::Render::TextureHandle m_cubeTextureA =
        ::SagaEngine::Render::TextureHandle::kInvalid;
    ::SagaEngine::Render::TextureHandle m_cubeTextureB =
        ::SagaEngine::Render::TextureHandle::kInvalid;
    ::SagaEngine::Render::TextureHandle m_groundTexture =
        ::SagaEngine::Render::TextureHandle::kInvalid;

    std::vector<CubeInstance> m_cubes;

    SagaEngine::Input::InputManager m_inputManager;
    bool m_cursorLocked = true;
    bool m_shadowsEnabled = true;
    ::SagaEngine::Math::Vec3 m_cameraPosition{0.0f, 2.4f, 7.0f};
    float m_yaw = 0.0f;
    float m_pitch = -0.22f;
    float m_lightAngle = 0.65f;
    float m_time = 0.0f;
};

} // namespace SagaSandbox
