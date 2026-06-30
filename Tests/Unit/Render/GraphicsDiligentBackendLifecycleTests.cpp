/// @file GraphicsDiligentBackendLifecycleTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include <gtest/gtest.h>

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

TEST(GraphicsDiligentBackendAdapter, ResourceMethodsRequireInitializedBackend)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    EXPECT_FALSE(backend->CreateTexture({}).IsValid());
    EXPECT_FALSE(backend->CreateBuffer({}).IsValid());
    EXPECT_FALSE(backend->CreateShader({}).IsValid());
    EXPECT_FALSE(backend->CreatePipeline({}).IsValid());
    EXPECT_FALSE(backend->CreateSampler({}).IsValid());

    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    const auto shader = backend->CreateShader(MakeShaderDesc());
    const auto pipeline = backend->CreatePipeline({});
    const auto sampler = backend->CreateSampler({});

    EXPECT_TRUE(texture.IsValid());
    EXPECT_TRUE(buffer.IsValid());
    EXPECT_TRUE(shader.IsValid());
    EXPECT_TRUE(pipeline.IsValid());
    EXPECT_TRUE(sampler.IsValid());

    backend->DestroyTexture({});
    backend->DestroyBuffer({});
    backend->DestroyShader({});
    backend->DestroyPipeline({});
    backend->DestroySampler({});

    backend->Shutdown();

    EXPECT_FALSE(backend->CreateTexture({}).IsValid());
    EXPECT_FALSE(backend->CreateBuffer({}).IsValid());
    EXPECT_FALSE(backend->CreateShader({}).IsValid());
    EXPECT_FALSE(backend->CreatePipeline({}).IsValid());
    EXPECT_FALSE(backend->CreateSampler({}).IsValid());
}
