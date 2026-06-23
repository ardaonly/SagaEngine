/// @file GraphicsDiligentBackendAdapterTests.cpp
/// @brief Tests private SagaGraphics lifecycle adapter over render backend.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
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
    std::uint32_t textureCreateCalls = 0;
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

Graphics::TextureDesc MakeTextureDesc() noexcept
{
    Graphics::TextureDesc desc{};
    desc.width = 4u;
    desc.height = 4u;
    desc.format = Graphics::ResourceFormat::Rgba8Unorm;
    return desc;
}

Graphics::BufferDesc MakeBufferDesc() noexcept
{
    Graphics::BufferDesc desc{};
    desc.sizeBytes = 128u;
    return desc;
}

Graphics::BufferDesc MakeNativeBufferDesc(
    Graphics::BufferUsage usage,
    std::uint64_t sizeBytes = 128u) noexcept
{
    Graphics::BufferDesc desc{};
    desc.debugName = "native-buffer";
    desc.sizeBytes = sizeBytes;
    desc.usage = usage;
    return desc;
}

Graphics::ShaderDesc MakeShaderDesc() noexcept
{
    Graphics::ShaderDesc desc{};
    desc.byteSize = 32u;
    return desc;
}

Graphics::GraphicsHandle ToGraphicsHandle(
    const Graphics::GraphicsHandle& handle) noexcept
{
    return handle;
}

Graphics::TextureHandle ToTextureHandle(
    Graphics::GraphicsHandle handle) noexcept
{
    Graphics::TextureHandle texture{};
    texture.index = handle.index;
    texture.generation = handle.generation;
    return texture;
}

Graphics::SamplerHandle ToSamplerHandle(
    Graphics::GraphicsHandle handle) noexcept
{
    Graphics::SamplerHandle sampler{};
    sampler.index = handle.index;
    sampler.generation = handle.generation;
    return sampler;
}

Graphics::BufferHandle MakeBufferHandle(
    std::uint32_t index,
    std::uint32_t generation) noexcept
{
    Graphics::BufferHandle handle{};
    handle.index = index;
    handle.generation = generation;
    return handle;
}

Graphics::GraphicsBindingLayoutDesc MakeTextureSamplerLayout()
{
    Graphics::GraphicsBindingLayoutDesc layout{};
    layout.slots.push_back({
        0u,
        Graphics::GraphicsBindingType::Texture,
        Graphics::kGraphicsShaderStageFragment,
        true,
        1u,
    });
    layout.slots.push_back({
        1u,
        Graphics::GraphicsBindingType::Sampler,
        Graphics::kGraphicsShaderStageFragment,
        false,
        Graphics::kInvalidGraphicsBindingSlot,
    });
    return layout;
}

Graphics::GraphicsBindingResourceRef MakeTextureBinding(
    Graphics::TextureHandle handle) noexcept
{
    return {
        0u,
        Graphics::GraphicsResourceKind::Texture,
        ToGraphicsHandle(handle),
    };
}

Graphics::GraphicsBindingResourceRef MakeSamplerBinding(
    Graphics::SamplerHandle handle) noexcept
{
    return {
        1u,
        Graphics::GraphicsResourceKind::Sampler,
        ToGraphicsHandle(handle),
    };
}

Graphics::GraphicsResourceQueryResult QueryDiligentBindingResource(
    void* userData,
    Graphics::GraphicsResourceKind kind,
    Graphics::GraphicsHandle handle)
{
    auto& backend =
        *static_cast<Adapter::DiligentGraphicsBackend*>(userData);

    return backend.QueryResource(kind, handle);
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
        ++m_State.textureCreateCalls;
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

std::unique_ptr<Adapter::DiligentGraphicsBackend> MakeConcreteBackend(
    FakeRenderState& state,
    Adapter::DiligentGraphicsBackend::StatusReader statusReader =
        ReadFakeStatus)
{
    return std::make_unique<Adapter::DiligentGraphicsBackend>(
        std::make_unique<FakeRenderBackend>(state),
        statusReader);
}

template <std::size_t N>
Graphics::GraphicsDataView MakeDataView(
    const std::array<std::uint8_t, N>& data) noexcept
{
    return {data.data(), static_cast<std::uint64_t>(data.size()), 0u, 0u};
}

std::unique_ptr<RenderBackend::IRenderBackend> ReturnNoBackend(
    const Graphics::RenderBackendDesc&)
{
    return {};
}

RenderBackend::DiligentDeviceServices MakeFakeDeviceServices() noexcept
{
    return {
        reinterpret_cast<Diligent::IRenderDevice*>(0x1),
        reinterpret_cast<Diligent::IDeviceContext*>(0x2),
        reinterpret_cast<Diligent::ISwapChain*>(0x3),
    };
}

void BindFakeNativeDeviceServices(
    Adapter::DiligentGraphicsBackend& backend) noexcept
{
    backend.BindNativeDeviceServicesForTesting(MakeFakeDeviceServices());
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

TEST(GraphicsDiligentBackendAdapter, ResourceRegistryRejectsStaleHandles)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto first = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(first.IsValid());
    EXPECT_EQ(first.index, 1u);
    EXPECT_EQ(first.generation, 1u);

    backend->DestroyTexture(first);

    const auto second = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(second.IsValid());
    EXPECT_EQ(second.index, first.index);
    EXPECT_EQ(second.generation, first.generation + 1u);

    backend->DestroyTexture(first);
    backend->DestroyTexture(second);
    backend->DestroyTexture(second);

    const auto third = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(third.IsValid());
    EXPECT_EQ(third.index, second.index);
    EXPECT_EQ(third.generation, second.generation + 1u);
}

TEST(GraphicsDiligentBackendAdapter, ResourceValidationRejectsInvalidDescriptions)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto texture = MakeTextureDesc();
    texture.width = 0u;
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);

    Graphics::BufferDesc buffer{};
    EXPECT_FALSE(backend->CreateBuffer(buffer).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);

    Graphics::ShaderDesc shader{};
    EXPECT_FALSE(backend->CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);

    Graphics::PipelineDesc pipeline{};
    pipeline.colorTargetCount = 0u;
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidPipelineDesc);

    Graphics::SamplerDesc sampler{};
    sampler.addressU = static_cast<Graphics::AddressMode>(255);
    EXPECT_FALSE(backend->CreateSampler(sampler).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidSamplerDesc);

    const auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.failedCreateCount, 5u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
}

TEST(GraphicsDiligentBackendAdapter, ResourceValidationRejectsRangeAndEnumEdges)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto texture = MakeTextureDesc();
    texture.depth = 0u;
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);

    texture = MakeTextureDesc();
    texture.mipLevels = 0u;
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());

    texture = MakeTextureDesc();
    texture.arrayLayers = 0u;
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());

    texture = MakeTextureDesc();
    texture.format = static_cast<Graphics::ResourceFormat>(255);
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());

    auto pipeline = Graphics::PipelineDesc{};
    pipeline.colorTargetCount = 9u;
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidPipelineDesc);

    pipeline = {};
    pipeline.colorFormat = static_cast<Graphics::ResourceFormat>(255);
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());

    pipeline = {};
    pipeline.depthFormat = Graphics::ResourceFormat::Unknown;
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());

    auto sampler = Graphics::SamplerDesc{};
    sampler.magFilter = static_cast<Graphics::FilterMode>(255);
    EXPECT_FALSE(backend->CreateSampler(sampler).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidSamplerDesc);

    sampler = {};
    sampler.addressV = static_cast<Graphics::AddressMode>(255);
    EXPECT_FALSE(backend->CreateSampler(sampler).IsValid());

    sampler = {};
    sampler.addressW = static_cast<Graphics::AddressMode>(255);
    EXPECT_FALSE(backend->CreateSampler(sampler).IsValid());

    const auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.failedCreateCount, 10u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
    EXPECT_EQ(state.textureCreateCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, FailedCreateUpdatesLastFailure)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    EXPECT_FALSE(backend->CreateTexture(MakeTextureDesc()).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::BackendNotInitialized);
    EXPECT_EQ(backend->GetResourceMemoryReport().failedCreateCount, 1u);

    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    Graphics::ShaderDesc shader{};
    shader.stage = static_cast<Graphics::ShaderStage>(255);
    shader.byteSize = 32u;
    EXPECT_FALSE(backend->CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);
    EXPECT_EQ(backend->GetResourceMemoryReport().failedCreateCount, 2u);
}

TEST(GraphicsDiligentBackendAdapter, InitializedCreateReturnsNativePreparedHandle)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
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
    EXPECT_EQ(state.beginFrameCalls, 0u);
    EXPECT_EQ(state.endFrameCalls, 0u);
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
}

TEST(GraphicsDiligentBackendAdapter, ResourceMemoryReportChangesOnCreateDestroy)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    const auto shader = backend->CreateShader(MakeShaderDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());
    ASSERT_TRUE(shader.IsValid());

    auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 1u);
    EXPECT_EQ(report.liveBufferCount, 1u);
    EXPECT_EQ(report.liveShaderCount, 1u);
    EXPECT_EQ(report.textureBytes, 64u);
    EXPECT_EQ(report.bufferBytes, 128u);
    EXPECT_EQ(report.shaderBytes, 32u);
    EXPECT_EQ(report.totalLiveBytes, 224u);
    EXPECT_EQ(report.peakLiveBytes, 224u);

    backend->DestroyTexture(texture);
    report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 0u);
    EXPECT_EQ(report.textureBytes, 0u);
    EXPECT_EQ(report.totalLiveBytes, 160u);
    EXPECT_EQ(report.peakLiveBytes, 224u);

    backend->DestroyTexture(texture);
    backend->DestroyTexture({});
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
    EXPECT_EQ(backend->GetResourceMemoryReport().totalLiveBytes, 160u);
}

TEST(GraphicsDiligentBackendAdapter, ShutdownClearsResourceReport)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    EXPECT_TRUE(backend->CreateTexture(MakeTextureDesc()).IsValid());
    EXPECT_TRUE(backend->CreateBuffer(MakeBufferDesc()).IsValid());
    ASSERT_NE(backend->GetResourceMemoryReport().totalLiveBytes, 0u);

    backend->Shutdown();

    const auto leakSummary = backend->GetLastShutdownResourceLeakSummary();
    EXPECT_TRUE(leakSummary.hadLiveResources);
    EXPECT_EQ(leakSummary.leakedTextureCount, 1u);
    EXPECT_EQ(leakSummary.leakedBufferCount, 1u);
    EXPECT_EQ(leakSummary.textureBytes, 64u);
    EXPECT_EQ(leakSummary.bufferBytes, 128u);
    EXPECT_EQ(leakSummary.totalLeakedBytes, 192u);

    const auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 0u);
    EXPECT_EQ(report.liveBufferCount, 0u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
    EXPECT_NE(report.peakLiveBytes, 0u);
    EXPECT_EQ(
        backend->GetStatus().health,
        Graphics::RenderBackendHealth::Shutdown);
}

TEST(GraphicsDiligentBackendAdapter, DestroyNoOpsDoNotUpdateFailure)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());
    backend->DestroyTexture(texture);

    const auto replacement = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(replacement.IsValid());
    ASSERT_NE(replacement.generation, texture.generation);
    backend->DestroyTexture(texture);
    backend->DestroyTexture({});
    backend->DestroyTexture(replacement);
    backend->DestroyTexture(replacement);
    backend->Shutdown();
    backend->DestroyTexture(replacement);

    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
    EXPECT_EQ(backend->GetResourceMemoryReport().failedCreateCount, 0u);
}

TEST(GraphicsDiligentBackendAdapter, NativePreparedHandlesReportBackingState)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    BindFakeNativeDeviceServices(*backend);

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());

    EXPECT_EQ(
        backend->GetTextureBackingForTesting(texture),
        Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_EQ(
        backend->GetBufferBackingForTesting(buffer),
        Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_NE(backend->GetTextureNativeSerialForTesting(texture), 0u);
    EXPECT_NE(backend->GetBufferNativeSerialForTesting(buffer), 0u);
    EXPECT_EQ(
        backend->GetTextureBackingForTesting({}),
        Graphics::GraphicsResourceBacking::Invalid);

    backend->DestroyTexture(texture);
    EXPECT_EQ(
        backend->GetTextureBackingForTesting(texture),
        Graphics::GraphicsResourceBacking::Invalid);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(texture), 0u);

    const auto nextTexture = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(nextTexture.IsValid());
    EXPECT_NE(backend->GetTextureNativeSerialForTesting(nextTexture), 0u);
    backend->Shutdown();
    EXPECT_EQ(
        backend->GetTextureBackingForTesting(nextTexture),
        Graphics::GraphicsResourceBacking::Invalid);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(nextTexture), 0u);
    EXPECT_EQ(state.textureCreateCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRejectsZeroSize)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Vertex);
    desc.sizeBytes = 0u;

    EXPECT_FALSE(backend->CreateBuffer(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRejectsOverflow)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    backend->BindNativeDeviceServicesForTesting(
        MakeFakeDeviceServices(),
        true);

    auto desc = MakeNativeBufferDesc(
        Graphics::BufferUsage::Vertex,
        std::numeric_limits<std::uint64_t>::max());

    EXPECT_FALSE(backend->CreateBuffer(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRejectsOversizedPayload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 16u> bytes{};
    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Vertex, 8u);

    EXPECT_FALSE(
        backend->CreateBuffer(desc, MakeDataView(bytes)).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRejectsInvalidUsage)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Storage);

    EXPECT_FALSE(backend->CreateBuffer(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRequiresBoundServices)
{
    RenderBackend::DiligentNativeResourceOwner owner;
    std::string diagnostic;
    const auto serial = owner.CreateBufferForHandle(
        MakeBufferHandle(1u, 1001u),
        MakeNativeBufferDesc(Graphics::BufferUsage::Vertex),
        {},
        &diagnostic);

    EXPECT_EQ(serial, 0u);
    EXPECT_FALSE(diagnostic.empty());
}

TEST(GraphicsDiligentBackendAdapter, FailedNativeCreationReturnsInvalidHandle)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    backend->BindNativeDeviceServicesForTesting(
        MakeFakeDeviceServices(),
        true);

    auto desc = MakeNativeBufferDesc(
        Graphics::BufferUsage::Vertex,
        std::numeric_limits<std::uint64_t>::max());
    const auto buffer = backend->CreateBuffer(desc);

    EXPECT_FALSE(buffer.IsValid());
    EXPECT_EQ(backend->GetResourceMemoryReport().liveBufferCount, 0u);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferQueryReportsNativeGpu)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Vertex);
    desc.debugName = "query-native-buffer";
    const auto buffer = backend->CreateBuffer(desc);
    ASSERT_TRUE(buffer.IsValid());

    ASSERT_TRUE(backend->MarkBufferNativeForTesting(buffer, 42u));

    const auto query = backend->QueryBufferForTesting(buffer);
    EXPECT_TRUE(query.valid);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Buffer);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpu);
    EXPECT_TRUE(query.nativeBacked);
    EXPECT_TRUE(query.resident);
    EXPECT_EQ(query.logicalBytes, desc.sizeBytes);
    EXPECT_EQ(query.debugName, "query-native-buffer");
}

TEST(GraphicsDiligentBackendAdapter, WrongKindBufferQueryIsRejected)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());

    const auto query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Texture,
        buffer);
    EXPECT_FALSE(query.valid);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
}

TEST(GraphicsDiligentBackendAdapter, StaleBufferQueryIsRejected)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());
    backend->DestroyBuffer(buffer);

    const auto query = backend->QueryBufferForTesting(buffer);
    EXPECT_FALSE(query.valid);
    EXPECT_FALSE(query.live);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferCarriesDebugName)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Uniform);
    desc.debugName = "camera-constants";
    const auto buffer = backend->CreateBuffer(desc);
    ASSERT_TRUE(buffer.IsValid());

    const auto query = backend->QueryBufferForTesting(buffer);
    EXPECT_EQ(query.debugName, "camera-constants");
}

TEST(
    GraphicsDiligentBackendAdapter,
    DeviceServicesAreOptionalUntilRuntimeBound)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    EXPECT_FALSE(backend->HasNativeDeviceServicesForTesting());

    const auto unboundTexture = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(unboundTexture.IsValid());

    auto query = backend->QueryTextureForTesting(unboundTexture);
    EXPECT_TRUE(query.valid);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(
        query.lifecycle,
        Graphics::GraphicsResourceLifecycle::RegisteredOnly);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_FALSE(query.nativeBacked);
    EXPECT_EQ(query.logicalBytes, 64u);
    EXPECT_EQ(query.approximateBytes, query.logicalBytes);
    EXPECT_FALSE(query.resident);
    EXPECT_FALSE(query.pendingDestroy);
    EXPECT_NE(query.creationSerial, 0u);
    EXPECT_EQ(query.lastUseSerial, 0u);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(unboundTexture), 0u);

    BindFakeNativeDeviceServices(*backend);
    EXPECT_TRUE(backend->HasNativeDeviceServicesForTesting());

    const auto boundBuffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(boundBuffer.IsValid());

    query = backend->QueryBufferForTesting(boundBuffer);
    EXPECT_TRUE(query.valid);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Buffer);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_FALSE(query.nativeBacked);
    EXPECT_EQ(query.logicalBytes, 128u);
    EXPECT_FALSE(query.resident);
    EXPECT_NE(query.creationSerial, 0u);
    EXPECT_NE(backend->GetBufferNativeSerialForTesting(boundBuffer), 0u);
}

TEST(GraphicsDiligentBackendAdapter, NativeBackendDoesNotCreateSecondDevice)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    EXPECT_EQ(state.initializeCalls, 1u);

    BindFakeNativeDeviceServices(*backend);
    EXPECT_TRUE(backend->HasNativeDeviceServicesForTesting());

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());

    EXPECT_EQ(state.initializeCalls, 1u);
    EXPECT_EQ(state.textureCreateCalls, 0u);
    EXPECT_EQ(state.beginFrameCalls, 0u);
    EXPECT_EQ(state.endFrameCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, ResourceQueriesRejectWrongKindAndStale)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());

    auto query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Texture,
        buffer);
    EXPECT_FALSE(query.valid);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Invalid);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::Invalid);

    backend->DestroyTexture(texture);
    query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Texture,
        texture);
    EXPECT_FALSE(query.valid);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
    EXPECT_EQ(query.logicalBytes, 0u);
    EXPECT_FALSE(query.pendingDestroy);

    const auto failed = backend->CreateBuffer({});
    EXPECT_FALSE(failed.IsValid());
    query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Buffer,
        failed);
    EXPECT_FALSE(query.valid);
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);
}

TEST(GraphicsDiligentBackendAdapter, QueryHelpersReportLiveKindAndBytes)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    BindFakeNativeDeviceServices(*backend);

    auto textureDesc = MakeTextureDesc();
    textureDesc.debugName = "albedo";
    auto bufferDesc = MakeBufferDesc();
    bufferDesc.debugName = "vertices";
    auto shaderDesc = MakeShaderDesc();
    shaderDesc.debugName = "solid-vs";
    Graphics::PipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "opaque-pipeline";
    Graphics::SamplerDesc samplerDesc{};
    samplerDesc.debugName = "linear-clamp";

    const auto texture = backend->CreateTexture(textureDesc);
    const auto buffer = backend->CreateBuffer(bufferDesc);
    const auto shader = backend->CreateShader(shaderDesc);
    const auto pipeline = backend->CreatePipeline(pipelineDesc);
    const auto sampler = backend->CreateSampler(samplerDesc);
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());
    ASSERT_TRUE(shader.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    auto query = backend->QueryTextureForTesting(texture);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_EQ(query.approximateBytes, 64u);
    EXPECT_EQ(query.debugName, "albedo");

    query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Texture,
        texture);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(query.debugName, "albedo");

    query = backend->QueryBufferForTesting(buffer);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Buffer);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_EQ(query.approximateBytes, 128u);
    EXPECT_EQ(query.debugName, "vertices");

    query = backend->QueryShaderForTesting(shader);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Shader);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(query.approximateBytes, 32u);
    EXPECT_EQ(query.debugName, "solid-vs");

    query = backend->QueryPipelineForTesting(pipeline);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Pipeline);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(query.approximateBytes, 0u);
    EXPECT_EQ(query.debugName, "opaque-pipeline");

    query = backend->QuerySamplerForTesting(sampler);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Sampler);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_EQ(query.approximateBytes, 0u);
    EXPECT_EQ(query.debugName, "linear-clamp");
    EXPECT_NE(backend->GetSamplerNativeSerialForTesting(sampler), 0u);

    backend->DestroyBuffer(buffer);
    query = backend->QueryBufferForTesting(buffer);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::Invalid);
    EXPECT_TRUE(query.debugName.empty());

    backend->Shutdown();
    query = backend->QueryTextureForTesting(texture);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.approximateBytes, 0u);
    EXPECT_EQ(state.textureCreateCalls, 0u);
}

TEST(
    GraphicsDiligentBackendAdapter,
    NativePreparedHandlesValidateBindingsWithoutUpload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    BindFakeNativeDeviceServices(*backend);

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto sampler = backend->CreateSampler({});
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back(MakeTextureBinding(texture));
    bindingSet.resources.push_back(MakeSamplerBinding(sampler));

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryDiligentBindingResource,
        backend.get());
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.code, Graphics::GraphicsBindingValidationCode::None);
    EXPECT_EQ(
        backend->QueryTextureForTesting(texture).backing,
        Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_EQ(
        backend->QuerySamplerForTesting(sampler).backing,
        Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_EQ(state.textureCreateCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, HeadlessResourceReportUsesRegisteredOnlyHandles)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);

    Graphics::RenderBackendDesc desc{};
    desc.headless = true;

    EXPECT_TRUE(backend->Initialize(desc, {}));
    EXPECT_EQ(state.initializeCalls, 0u);

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());
    const auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 1u);
    EXPECT_EQ(report.textureBytes, 64u);
    EXPECT_EQ(
        backend->GetTextureBackingForTesting(texture),
        Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(texture), 0u);
    EXPECT_EQ(state.initializeCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, InitialDataAcceptedWithoutNativeUpload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    BindFakeNativeDeviceServices(*backend);

    std::array<std::uint8_t, 64> pixels{};
    const auto texture =
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(pixels));
    ASSERT_TRUE(texture.IsValid());
    EXPECT_EQ(backend->GetTextureShadowBytesForTesting(texture), 64u);
    EXPECT_TRUE(backend->GetTextureNativeUploadDeferredForTesting(texture));
    EXPECT_EQ(state.textureCreateCalls, 0u);

    std::array<std::uint8_t, 128> bytes{};
    const auto buffer =
        backend->CreateBuffer(MakeBufferDesc(), MakeDataView(bytes));
    ASSERT_TRUE(buffer.IsValid());
    EXPECT_EQ(backend->GetBufferShadowBytesForTesting(buffer), 128u);
    EXPECT_TRUE(backend->GetBufferNativeUploadDeferredForTesting(buffer));
}

TEST(GraphicsDiligentBackendAdapter, InvalidInitialDataUpdatesLastFailure)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 63> partialPixels{};
    EXPECT_FALSE(
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(partialPixels))
            .IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);

    Graphics::GraphicsDataView nullPayload{};
    nullPayload.sizeBytes = 1u;
    EXPECT_FALSE(
        backend->CreateBuffer(MakeBufferDesc(), nullPayload).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
    EXPECT_EQ(backend->GetResourceMemoryReport().failedCreateCount, 2u);
}

TEST(GraphicsDiligentBackendAdapter, InitialDataDoesNotTouchRenderBackendUploadPath)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    BindFakeNativeDeviceServices(*backend);

    std::array<std::uint8_t, 64> pixels{};
    EXPECT_TRUE(
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(pixels))
            .IsValid());
    EXPECT_EQ(state.textureCreateCalls, 0u);
    EXPECT_EQ(state.beginFrameCalls, 0u);
    EXPECT_EQ(state.endFrameCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, DestroyAndShutdownReleaseShadowPayload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    BindFakeNativeDeviceServices(*backend);

    std::array<std::uint8_t, 64> pixels{};
    const auto texture =
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(pixels));
    ASSERT_TRUE(texture.IsValid());
    ASSERT_EQ(backend->GetTextureShadowBytesForTesting(texture), 64u);

    backend->DestroyTexture(texture);
    EXPECT_EQ(backend->GetTextureShadowBytesForTesting(texture), 0u);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(texture), 0u);

    const auto nextTexture =
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(pixels));
    ASSERT_TRUE(nextTexture.IsValid());
    ASSERT_EQ(backend->GetTextureShadowBytesForTesting(nextTexture), 64u);
    ASSERT_NE(backend->GetTextureNativeSerialForTesting(nextTexture), 0u);

    backend->Shutdown();
    EXPECT_EQ(backend->GetTextureShadowBytesForTesting(nextTexture), 0u);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(nextTexture), 0u);
    EXPECT_EQ(backend->GetResourceMemoryReport().totalLiveBytes, 0u);
}
