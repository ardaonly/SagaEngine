/// @file GraphicsDiligentBackendTestHelpers.h
/// @brief Shared helpers for Diligent graphics adapter tests.

#pragma once

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

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

Graphics::TextureDesc MakeNativeTextureDesc() noexcept
{
    auto desc = MakeTextureDesc();
    desc.debugName = "native-texture";
    desc.format = Graphics::ResourceFormat::Rgba8Unorm;
    desc.dimension = Graphics::TextureDimension::Texture2D;
    desc.depth = 1u;
    desc.mipLevels = 1u;
    desc.arrayLayers = 1u;
    desc.usage = Graphics::TextureUsageFlags::Sampled;
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

Graphics::ShaderDesc MakeNativeShaderDesc(
    Graphics::ShaderStage stage = Graphics::ShaderStage::Vertex) noexcept
{
    Graphics::ShaderDesc desc{};
    desc.debugName = stage == Graphics::ShaderStage::Vertex
                         ? "native-vs"
                         : "native-ps";
    desc.stage = stage;
    desc.entryPoint = "main";
    desc.sourceIdentity = desc.debugName;
    desc.source = "void main() {}";
    return desc;
}

Graphics::SamplerDesc MakePointClampSamplerDesc() noexcept
{
    Graphics::SamplerDesc desc{};
    desc.debugName = "point-clamp";
    desc.minFilter = Graphics::FilterMode::Nearest;
    desc.magFilter = Graphics::FilterMode::Nearest;
    desc.addressU = Graphics::AddressMode::ClampToEdge;
    desc.addressV = Graphics::AddressMode::ClampToEdge;
    desc.addressW = Graphics::AddressMode::ClampToEdge;
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
    Graphics::GraphicsBindingLayoutSlot texture{};
    texture.slot = 0u;
    texture.stableId = 100u;
    texture.type = Graphics::GraphicsBindingType::SampledTexture;
    texture.stages = Graphics::kGraphicsShaderStageFragment;
    texture.required = true;
    texture.pairedSamplerSlot = 1u;
    layout.slots.push_back(texture);

    Graphics::GraphicsBindingLayoutSlot sampler{};
    sampler.slot = 1u;
    sampler.stableId = 101u;
    sampler.type = Graphics::GraphicsBindingType::Sampler;
    sampler.stages = Graphics::kGraphicsShaderStageFragment;
    sampler.required = false;
    layout.slots.push_back(sampler);
    return layout;
}

Graphics::GraphicsBindingResourceRef MakeTextureBinding(
    Graphics::TextureHandle handle) noexcept
{
    return {
        0u,
        100u,
        Graphics::GraphicsResourceKind::Texture,
        ToGraphicsHandle(handle),
    };
}

Graphics::GraphicsBindingResourceRef MakeSamplerBinding(
    Graphics::SamplerHandle handle) noexcept
{
    return {
        1u,
        101u,
        Graphics::GraphicsResourceKind::Sampler,
        ToGraphicsHandle(handle),
    };
}

Graphics::GraphicsBindingSetDesc MakeTextureSet(
    Graphics::BindingLayoutHandle layout,
    Graphics::TextureHandle texture,
    Graphics::SamplerHandle sampler)
{
    Graphics::GraphicsBindingSetDesc desc{};
    desc.layout = layout;
    desc.resources.push_back(MakeTextureBinding(texture));
    desc.resources.push_back(MakeSamplerBinding(sampler));
    return desc;
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

using TestStatusReader = RenderBackend::RenderBackendStatus (*)(
    const RenderBackend::IRenderBackend&) noexcept;

std::unique_ptr<Graphics::IGraphicsBackend> MakeBackend(
    FakeRenderState& state,
    TestStatusReader statusReader = ReadFakeStatus)
{
    (void)state;
    (void)statusReader;
    return Adapter::CreateDiligentGraphicsBackendForTesting();
}

std::unique_ptr<Adapter::DiligentGraphicsBackend> MakeConcreteBackend(
    FakeRenderState& state,
    TestStatusReader statusReader = ReadFakeStatus)
{
    (void)state;
    (void)statusReader;
    return std::make_unique<Adapter::DiligentGraphicsBackend>(false, true);
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


} // namespace
