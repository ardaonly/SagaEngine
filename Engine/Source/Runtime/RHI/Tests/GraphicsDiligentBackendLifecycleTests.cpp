/// @file GraphicsDiligentBackendLifecycleTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include <gtest/gtest.h>

TEST(GraphicsDiligentBackendAdapter, MapsInitializeDescriptorsToHeadlessRuntime)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    Graphics::RenderBackendDesc backendDesc{};
    backendDesc.preferredBackend = Graphics::BackendPreference::Compatibility;
    backendDesc.enableValidation = true;

    const auto swapchain = MakeSwapchain();

    EXPECT_TRUE(backend->Initialize(backendDesc, swapchain));
    const auto status = backend->GetStatus();
    EXPECT_EQ(
        status.selectedBackend,
        Graphics::BackendPreference::Compatibility);
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Headless);
}

TEST(GraphicsDiligentBackendAdapter, AdvancesHeadlessFrameLifecycle)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    backend->Resize(1920u, 1080u);
    backend->BeginFrame();
    backend->EndFrame();
    backend->Shutdown();

    const auto status = backend->GetStatus();
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Shutdown);
}

TEST(GraphicsDiligentBackendAdapter, MapsRenderStatusToGraphicsStatus)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto status = backend->GetStatus();
    EXPECT_EQ(
        status.selectedBackend,
        Graphics::BackendPreference::Auto);
    EXPECT_EQ(status.frameIndex, 0u);
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(status.failure, Graphics::RenderBackendFailure::None);

    const auto capabilities = backend->GetCapabilities();
    EXPECT_EQ(
        capabilities.backend,
        Graphics::BackendPreference::Auto);
    EXPECT_EQ(
        capabilities.qualityCeiling,
        Graphics::RenderQualityPreset::Low);
    EXPECT_EQ(
        capabilities.compute,
        Graphics::RenderFeatureSupport::Unsupported);
    EXPECT_EQ(capabilities.maxTexture2DSize, 1024u);
}

TEST(GraphicsDiligentBackendAdapter, ForcedHeadlessInitializeIgnoresFakeFailure)
{
    FakeRenderState state;
    state.initializeResult = false;
    auto backend = MakeBackend(state, ReadFailedFakeStatus);

    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    const auto status = backend->GetStatus();
    EXPECT_EQ(
        status.selectedBackend,
        Graphics::BackendPreference::Auto);
    EXPECT_EQ(status.frameIndex, 0u);
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::None);

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

TEST(GraphicsDiligentBackendAdapter, TestingFactoryUsesHeadlessRuntime)
{
    auto backend = Adapter::CreateDiligentGraphicsBackendForTesting();

    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    const auto status = backend->GetStatus();
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::None);

    const auto capabilities = backend->GetCapabilities();
    EXPECT_EQ(
        capabilities.qualityCeiling,
        Graphics::RenderQualityPreset::Low);
    EXPECT_EQ(capabilities.rayTracing, Graphics::RenderFeatureSupport::Unsupported);
}

TEST(GraphicsDiligentBackendAdapter, ZeroSizeInitializeSkipsBackend)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    Graphics::SwapchainDesc swapchain{};
    swapchain.width = 0u;
    swapchain.height = 720u;

    EXPECT_TRUE(backend->Initialize({}, swapchain));
    EXPECT_EQ(state.initializeCalls, 0u);

    const auto status = backend->GetStatus();
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::None);

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
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::None);
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
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    backend->BeginFrame();
    backend->EndFrame();

    const auto status = backend->GetStatus();
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.health, Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(
        status.failure,
        Graphics::RenderBackendFailure::None);
}

TEST(GraphicsDiligentBackendAdapter, ShutdownAfterFailedInitializeIsIdempotent)
{
    FakeRenderState state;
    state.initializeResult = false;
    auto backend = MakeBackend(state, ReadFailedFakeStatus);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    backend->Shutdown();
    backend->Shutdown();

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
