/// @file First3DVerticalSliceScenario.h
/// @brief Small playable 3D render slice for the canonical Diligent path.

#pragma once

#include "SagaSandbox/Core/IScenario.h"

#include <SagaEngine/Input/Core/InputManager.h>
#include <SagaEngine/Math/Mat4.h>
#include <SagaEngine/Math/Vec3.h>
#include <SagaEngine/Render/Materials/Material.h>

#include <functional>
#include <vector>

namespace SagaSandbox
{

class First3DVerticalSliceScenario final : public IScenario
{
public:
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "first_3d_vertical_slice",
        .displayName = "First 3D Vertical Slice",
        .category    = "Render",
        .description = "Ground plane, textured cubes, depth, transforms, and fly-camera controls.",
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
    ::SagaEngine::Math::Vec3 m_cameraPosition{0.0f, 2.0f, 6.0f};
    float m_yaw = 0.0f;
    float m_pitch = -0.18f;
};

} // namespace SagaSandbox
