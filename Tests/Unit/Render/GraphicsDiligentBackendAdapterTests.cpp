/// @file GraphicsDiligentBackendAdapterTests.cpp
/// @brief Tests private SagaGraphics lifecycle adapter over render backend.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

namespace
{

namespace Graphics = SagaEngine::Graphics;
namespace Adapter = SagaEngine::Graphics::Backends::Diligent;
namespace RenderBackend = SagaEngine::Render::Backend;
namespace Render = SagaEngine::Render;
namespace RenderScene = SagaEngine::Render::Scene;
namespace RenderWorld = SagaEngine::Render::World;

struct FakeRenderState
{
    RenderBackend::SwapchainDesc lastSwapchain{};
    std::uint32_t resizeWidth = 0;
    std::uint32_t resizeHeight = 0;
    std::uint32_t initializeCalls = 0;
    std::uint32_t shutdownCalls = 0;
    std::uint32_t resizeCalls = 0;
    std::uint32_t beginFrameCalls = 0;
    std::uint32_t endFrameCalls = 0;
    bool initializeResult = true;
    bool initialized = false;
    RenderBackend::GraphicsBackendAPI selectedAPI =
        RenderBackend::GraphicsBackendAPI::kCompatibility;
};

Graphics::SwapchainDesc MakeSwapchain() noexcept
{
    Graphics::SwapchainDesc swapchain{};
    swapchain.nativeWindow = reinterpret_cast<void*>(0x1234);
    swapchain.width = 1280u;
    swapchain.height = 720u;
    swapchain.vsync = false;
    swapchain.highDynamicRange = true;
    return swapchain;
}

class FakeRenderBackend final : public RenderBackend::IRenderBackend
{
public:
    explicit FakeRenderBackend(FakeRenderState& state)
        : m_State(state)
    {
    }

    bool Initialize(const RenderBackend::SwapchainDesc& desc) override
    {
        ++m_State.initializeCalls;
        m_State.lastSwapchain = desc;
        m_State.initialized = m_State.initializeResult;
        return m_State.initializeResult;
    }

    void Shutdown() override
    {
        ++m_State.shutdownCalls;
        m_State.initialized = false;
    }

    void OnResize(std::uint32_t width, std::uint32_t height) override
    {
        ++m_State.resizeCalls;
        m_State.resizeWidth = width;
        m_State.resizeHeight = height;
    }

    RenderWorld::MeshId CreateMesh(const Render::MeshAsset&) override
    {
        return RenderWorld::MeshId::kInvalid;
    }

    RenderWorld::MaterialId CreateMaterial(
        const Render::MaterialRuntime&) override
    {
        return RenderWorld::MaterialId::kInvalid;
    }

    void DestroyMesh(RenderWorld::MeshId) override {}
    void DestroyMaterial(RenderWorld::MaterialId) override {}

    Render::TextureHandle CreateTexture(
        std::uint32_t,
        std::uint32_t,
        const std::uint8_t*) override
    {
        return {};
    }

    void DestroyTexture(Render::TextureHandle) override {}

    void BeginFrame() override
    {
        ++m_State.beginFrameCalls;
    }

    void Submit(
        const RenderScene::Camera&,
        const RenderScene::RenderView&) override
    {
    }

    void EndFrame() override
    {
        ++m_State.endFrameCalls;
    }

private:
    FakeRenderState& m_State;
};

RenderBackend::RenderBackendStatus ReadFakeStatus(
    const RenderBackend::IRenderBackend&) noexcept
{
    return {
        RenderBackend::GraphicsBackendAPI::kCompatibility,
        7u,
        true,
    };
}

RenderBackend::RenderBackendStatus ReadFailedFakeStatus(
    const RenderBackend::IRenderBackend&) noexcept
{
    return {
        RenderBackend::GraphicsBackendAPI::kNativePortable,
        0u,
        false,
    };
}

std::unique_ptr<Graphics::IGraphicsBackend> MakeBackend(
    FakeRenderState& state,
    Adapter::DiligentGraphicsBackend::StatusReader statusReader =
        ReadFakeStatus)
{
    return Adapter::CreateDiligentGraphicsBackendForTesting(
        std::make_unique<FakeRenderBackend>(state),
        statusReader);
}

std::unique_ptr<RenderBackend::IRenderBackend> ReturnNoBackend(
    const Graphics::RenderBackendDesc&)
{
    return {};
}

} // namespace

TEST(GraphicsDiligentBackendAdapter, MapsInitializeDescriptorsToRenderBackend)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    Graphics::RenderBackendDesc backendDesc{};
    backendDesc.preferredBackend = Graphics::BackendPreference::Compatibility;
    backendDesc.enableValidation = true;

    const auto swapchain = MakeSwapchain();

    EXPECT_TRUE(backend->Initialize(backendDesc, swapchain));
    EXPECT_EQ(state.initializeCalls, 1u);
    EXPECT_EQ(state.lastSwapchain.nativeWindow, swapchain.nativeWindow);
    EXPECT_EQ(state.lastSwapchain.width, 1280u);
    EXPECT_EQ(state.lastSwapchain.height, 720u);
    EXPECT_FALSE(state.lastSwapchain.vsync);
    EXPECT_TRUE(state.lastSwapchain.hdr);
}

TEST(GraphicsDiligentBackendAdapter, DelegatesLifecycleToRenderBackend)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    backend->Resize(1920u, 1080u);
    backend->BeginFrame();
    backend->EndFrame();
    backend->Shutdown();

    EXPECT_EQ(state.resizeCalls, 1u);
    EXPECT_EQ(state.resizeWidth, 1920u);
    EXPECT_EQ(state.resizeHeight, 1080u);
    EXPECT_EQ(state.beginFrameCalls, 1u);
    EXPECT_EQ(state.endFrameCalls, 1u);
    EXPECT_EQ(state.shutdownCalls, 2u);
}

TEST(GraphicsDiligentBackendAdapter, MapsRenderStatusToGraphicsStatus)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto status = backend->GetStatus();
    EXPECT_EQ(
        status.selectedBackend,
        Graphics::BackendPreference::Compatibility);
    EXPECT_EQ(status.frameIndex, 7u);
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Ready);
    EXPECT_EQ(status.failure, Graphics::RenderBackendFailure::None);

    const auto capabilities = backend->GetCapabilities();
    EXPECT_EQ(
        capabilities.backend,
        Graphics::BackendPreference::Compatibility);
    EXPECT_EQ(
        capabilities.qualityCeiling,
        Graphics::RenderQualityPreset::Medium);
    EXPECT_EQ(
        capabilities.compute,
        Graphics::RenderFeatureSupport::Unsupported);
    EXPECT_EQ(capabilities.maxTexture2DSize, 4096u);
}

TEST(GraphicsDiligentBackendAdapter, FailedInitializeReportsFailureStatus)
{
    FakeRenderState state;
    state.initializeResult = false;
    auto backend = MakeBackend(state, ReadFailedFakeStatus);

    EXPECT_FALSE(backend->Initialize({}, MakeSwapchain()));
    const auto status = backend->GetStatus();
    EXPECT_EQ(
        status.selectedBackend,
        Graphics::BackendPreference::NativePortable);
    EXPECT_EQ(status.frameIndex, 0u);
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Failed);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::InitializationFailed);

    const auto capabilities = backend->GetCapabilities();
    EXPECT_EQ(
        capabilities.qualityCeiling,
        Graphics::RenderQualityPreset::Low);
    EXPECT_EQ(capabilities.maxTexture2DSize, 1024u);
}

TEST(GraphicsDiligentBackendAdapter, HeadlessInitializeDoesNotTouchRenderBackend)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    Graphics::RenderBackendDesc desc{};
    desc.preferredBackend = Graphics::BackendPreference::NativePortable;
    desc.headless = true;

    EXPECT_TRUE(backend->Initialize(desc, {}));
    EXPECT_EQ(state.initializeCalls, 0u);

    backend->BeginFrame();
    const auto status = backend->GetStatus();
    EXPECT_EQ(
        status.selectedBackend,
        Graphics::BackendPreference::NativePortable);
    EXPECT_EQ(status.frameIndex, 1u);
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(status.failure, Graphics::RenderBackendFailure::None);

    const auto capabilities = backend->GetCapabilities();
    EXPECT_EQ(
        capabilities.backend,
        Graphics::BackendPreference::NativePortable);
    EXPECT_EQ(
        capabilities.qualityCeiling,
        Graphics::RenderQualityPreset::Low);
}

TEST(GraphicsDiligentBackendAdapter, NullFactoryReportsBackendUnavailable)
{
    auto backend = Adapter::CreateDiligentGraphicsBackendForTesting(
        ReturnNoBackend,
        ReadFakeStatus);

    EXPECT_FALSE(backend->Initialize({}, MakeSwapchain()));
    const auto status = backend->GetStatus();
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Failed);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::BackendUnavailable);

    const auto capabilities = backend->GetCapabilities();
    EXPECT_EQ(
        capabilities.qualityCeiling,
        Graphics::RenderQualityPreset::Low);
    EXPECT_EQ(
        capabilities.rayTracing,
        Graphics::RenderFeatureSupport::Unsupported);
}

TEST(GraphicsDiligentBackendAdapter, ZeroSizeInitializeSkipsBackend)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    Graphics::SwapchainDesc swapchain{};
    swapchain.width = 0u;
    swapchain.height = 720u;

    EXPECT_FALSE(backend->Initialize({}, swapchain));
    EXPECT_EQ(state.initializeCalls, 0u);

    const auto status = backend->GetStatus();
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::FrameSkipped);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::InvalidSurfaceSize);

    const auto capabilities = backend->GetCapabilities();
    EXPECT_EQ(
        capabilities.qualityCeiling,
        Graphics::RenderQualityPreset::Low);
}

TEST(GraphicsDiligentBackendAdapter, ZeroSizeResizeSkipsBackendResize)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    backend->Resize(0u, 1080u);
    EXPECT_EQ(state.resizeCalls, 0u);

    const auto status = backend->GetStatus();
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::FrameSkipped);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::InvalidSurfaceSize);
}

TEST(GraphicsDiligentBackendAdapter, FrameCallsBeforeInitializeAreNoOps)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    backend->BeginFrame();
    backend->EndFrame();

    EXPECT_EQ(state.beginFrameCalls, 0u);
    EXPECT_EQ(state.endFrameCalls, 0u);
    const auto status = backend->GetStatus();
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::FrameSkipped);
}

TEST(GraphicsDiligentBackendAdapter, FrameCallsAfterFailedInitializeAreNoOps)
{
    FakeRenderState state;
    state.initializeResult = false;
    auto backend = MakeBackend(state, ReadFailedFakeStatus);
    EXPECT_FALSE(backend->Initialize({}, MakeSwapchain()));

    backend->BeginFrame();
    backend->EndFrame();

    EXPECT_EQ(state.beginFrameCalls, 0u);
    EXPECT_EQ(state.endFrameCalls, 0u);
    const auto status = backend->GetStatus();
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::FrameSkipped);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::InitializationFailed);
}

TEST(GraphicsDiligentBackendAdapter, ShutdownAfterFailedInitializeIsIdempotent)
{
    FakeRenderState state;
    state.initializeResult = false;
    auto backend = MakeBackend(state, ReadFailedFakeStatus);
    EXPECT_FALSE(backend->Initialize({}, MakeSwapchain()));

    backend->Shutdown();
    backend->Shutdown();

    EXPECT_EQ(state.shutdownCalls, 4u);
    const auto status = backend->GetStatus();
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Shutdown);

    const auto capabilities = backend->GetCapabilities();
    EXPECT_EQ(
        capabilities.qualityCeiling,
        Graphics::RenderQualityPreset::Low);
}

TEST(GraphicsDiligentBackendAdapter, ResourceMethodsReturnInvalidHandlesInV0)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    EXPECT_FALSE(backend->CreateTexture({}).IsValid());
    EXPECT_FALSE(backend->CreateBuffer({}).IsValid());
    EXPECT_FALSE(backend->CreateShader({}).IsValid());
    EXPECT_FALSE(backend->CreatePipeline({}).IsValid());
    EXPECT_FALSE(backend->CreateSampler({}).IsValid());

    backend->DestroyTexture({});
    backend->DestroyBuffer({});
    backend->DestroyShader({});
    backend->DestroyPipeline({});
    backend->DestroySampler({});
}
