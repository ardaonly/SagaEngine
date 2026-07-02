#pragma once

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"
#include "SagaEngine/Platform/IWindow.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"
#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Materials/MeshAsset.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/RenderView.h"
#include "Render/DiligentPixelTestHelpers.h"

#include <gtest/gtest.h>

#include <cstdint>

struct SDL_Window;

[[nodiscard]] void* GetNativeHandle(SDL_Window* window);

class TestDiligentRenderBackend
{
public:
    TestDiligentRenderBackend();

    [[nodiscard]] bool Initialize(const SagaEngine::Render::Backend::SwapchainDesc& desc);
    void Shutdown();
    void OnResize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] SagaEngine::Render::World::MeshId CreateMesh(
        const SagaEngine::Render::MeshAsset& asset);
    [[nodiscard]] SagaEngine::Render::World::MaterialId CreateMaterial(
        const SagaEngine::Render::MaterialRuntime& runtime);
    void DestroyMesh(SagaEngine::Render::World::MeshId id);
    void DestroyMaterial(SagaEngine::Render::World::MaterialId id);

    [[nodiscard]] SagaEngine::Render::TextureHandle CreateTexture(
        std::uint32_t width,
        std::uint32_t height,
        const std::uint8_t* rgba);
    void DestroyTexture(SagaEngine::Render::TextureHandle texture);

    void BeginFrame();
    void Submit(
        const SagaEngine::Render::Scene::Camera& camera,
        const SagaEngine::Render::Scene::RenderView& view);
    void EndFrame();

    [[nodiscard]] SagaEngine::Render::Backend::GraphicsBackendAPI SelectedAPI() const noexcept;
    [[nodiscard]] std::uint64_t FrameIndex() const noexcept;
    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] SagaEngine::Render::Backend::RenderFrameDiagnostics LastFrameDiagnostics() const noexcept;
    [[nodiscard]] SagaEngine::Render::Backend::DiligentDeviceServices GetDiligentDeviceServices() const noexcept;
    [[nodiscard]] SagaEngine::Render::Backend::RenderCaptureResult CaptureCurrentColorFrame(
        SagaEngine::Render::Backend::RenderFrameCapture& capture);
    [[nodiscard]] SagaEngine::Render::Backend::IRenderBackend& BackendInterface() noexcept;

private:
    static SagaEngine::Render::Backend::RenderBackendConfig MakeConfig() noexcept;

    SagaEngine::Render::Backend::DiligentRenderBackend m_backend;
};

class DiligentGPU : public ::testing::Test
{
protected:
    SDL_Window* m_Window = nullptr;
    TestDiligentRenderBackend m_Backend;
    bool m_HasGPU = false;

    static constexpr std::uint32_t kWidth = 320;
    static constexpr std::uint32_t kHeight = 240;

    void SetUp() override;
    void TearDown() override;

    void PumpOneFrame();

    [[nodiscard]] SagaEngine::Render::Backend::RenderFrameCapture RenderAndCapture(
        const SagaEngine::Render::Scene::Camera& camera,
        const SagaEngine::Render::Scene::RenderView& view);

    [[nodiscard]] SagaEngine::Render::World::MaterialId CreateSolidMaterial(
        std::uint64_t materialId,
        SagaTests::Render::Rgba8 color,
        SagaEngine::Render::TextureHandle* outTexture = nullptr,
        SagaEngine::Render::OpaqueShadingModel shadingModel =
            SagaEngine::Render::OpaqueShadingModel::Unlit);

    [[nodiscard]] SagaEngine::Render::Scene::RenderView MakeSingleDrawView(
        SagaEngine::Render::World::MeshId mesh,
        SagaEngine::Render::World::MaterialId material,
        SagaEngine::Math::Mat4 model = SagaEngine::Math::Mat4::Identity());
};

class CoordinateGPU : public DiligentGPU
{
};
