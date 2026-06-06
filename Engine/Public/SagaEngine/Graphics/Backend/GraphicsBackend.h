/// @file GraphicsBackend.h
/// @brief Minimal vendor-neutral graphics backend shell.

#pragma once

#include "SagaEngine/Graphics/Core/GraphicsTypes.h"
#include "SagaEngine/Graphics/Descs/ResourceDescs.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

#include <cstdint>

namespace SagaEngine::Graphics
{

struct SwapchainDesc
{
    void*         nativeWindow = nullptr;
    std::uint32_t width        = 0;
    std::uint32_t height       = 0;
    bool          vsync        = true;
    bool          highDynamicRange = false;
};

struct RenderBackendDesc
{
    BackendPreference preferredBackend = BackendPreference::Auto;
    bool              enableValidation = false;
    bool              headless         = false;
};

enum class RenderBackendHealth : std::uint8_t
{
    Uninitialized = 0,
    Ready,
    Headless,
    FrameSkipped,
    Failed,
    Shutdown,
};

enum class RenderBackendFailure : std::uint8_t
{
    None = 0,
    BackendUnavailable,
    InitializationFailed,
    InvalidSurfaceSize,
};

enum class RenderQualityPreset : std::uint8_t
{
    Low = 0,
    Medium,
    High,
    Ultra,
};

enum class RenderFeatureSupport : std::uint8_t
{
    Unsupported = 0,
    Supported,
};

enum class RenderCapabilityFallback : std::uint8_t
{
    NotRequested = 0,
    Available,
    DisabledUnsupported,
};

struct RenderBackendStatus
{
    BackendPreference    selectedBackend = BackendPreference::Auto;
    std::uint64_t        frameIndex      = 0;
    bool                 initialized     = false;
    RenderBackendHealth  health          = RenderBackendHealth::Uninitialized;
    RenderBackendFailure failure         = RenderBackendFailure::None;
};

struct RenderBackendCapabilities
{
    BackendPreference    backend = BackendPreference::Auto;
    RenderQualityPreset  qualityCeiling = RenderQualityPreset::Low;
    RenderFeatureSupport compute = RenderFeatureSupport::Unsupported;
    RenderFeatureSupport timestampQueries = RenderFeatureSupport::Unsupported;
    RenderFeatureSupport bindlessResourceArrays =
        RenderFeatureSupport::Unsupported;
    RenderFeatureSupport rayTracing = RenderFeatureSupport::Unsupported;
    RenderFeatureSupport hdrSwapchain = RenderFeatureSupport::Unsupported;
    RenderFeatureSupport textureCompressionBC =
        RenderFeatureSupport::Unsupported;
    RenderFeatureSupport textureCompressionASTC =
        RenderFeatureSupport::Unsupported;
    RenderFeatureSupport textureCompressionETC2 =
        RenderFeatureSupport::Unsupported;
    RenderFeatureSupport meshShaders = RenderFeatureSupport::Unsupported;
    RenderFeatureSupport indirectDraw = RenderFeatureSupport::Unsupported;
    RenderFeatureSupport conservativeRaster =
        RenderFeatureSupport::Unsupported;
    RenderFeatureSupport depthBounds = RenderFeatureSupport::Unsupported;
    std::uint32_t maxTexture2DSize = 0;
    std::uint32_t maxColorAttachments = 0;
    std::uint32_t maxFramesInFlight = 0;
};

[[nodiscard]] constexpr RenderQualityPreset ClampRenderQualityPreset(
    RenderQualityPreset requested,
    const RenderBackendCapabilities& capabilities) noexcept
{
    return static_cast<std::uint8_t>(requested) >
                   static_cast<std::uint8_t>(capabilities.qualityCeiling)
               ? capabilities.qualityCeiling
               : requested;
}

[[nodiscard]] constexpr RenderCapabilityFallback ResolveRenderCapabilityFallback(
    RenderFeatureSupport support,
    bool requested) noexcept
{
    if (!requested)
    {
        return RenderCapabilityFallback::NotRequested;
    }

    return support == RenderFeatureSupport::Supported
               ? RenderCapabilityFallback::Available
               : RenderCapabilityFallback::DisabledUnsupported;
}

class IGraphicsBackend
{
public:
    virtual ~IGraphicsBackend() = default;

    [[nodiscard]] virtual bool Initialize(
        const RenderBackendDesc& backend,
        const SwapchainDesc& swapchain) = 0;
    virtual void Shutdown() = 0;
    virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;

    [[nodiscard]] virtual TextureHandle CreateTexture(
        const TextureDesc& desc) = 0;
    [[nodiscard]] virtual BufferHandle CreateBuffer(
        const BufferDesc& desc) = 0;
    [[nodiscard]] virtual ShaderHandle CreateShader(
        const ShaderDesc& desc) = 0;
    [[nodiscard]] virtual PipelineHandle CreatePipeline(
        const PipelineDesc& desc) = 0;
    [[nodiscard]] virtual SamplerHandle CreateSampler(
        const SamplerDesc& desc) = 0;

    virtual void DestroyTexture(TextureHandle handle) = 0;
    virtual void DestroyBuffer(BufferHandle handle) = 0;
    virtual void DestroyShader(ShaderHandle handle) = 0;
    virtual void DestroyPipeline(PipelineHandle handle) = 0;
    virtual void DestroySampler(SamplerHandle handle) = 0;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    [[nodiscard]] virtual RenderBackendStatus GetStatus() const noexcept = 0;
    [[nodiscard]] virtual RenderBackendCapabilities
    GetCapabilities() const noexcept = 0;
};

class NullGraphicsBackend final : public IGraphicsBackend
{
public:
    bool Initialize(
        const RenderBackendDesc& backend,
        const SwapchainDesc&) override
    {
        m_Status.selectedBackend = backend.preferredBackend;
        m_Status.initialized = true;
        m_Status.health =
            backend.headless ? RenderBackendHealth::Headless
                             : RenderBackendHealth::Ready;
        m_Status.failure = RenderBackendFailure::None;
        return true;
    }

    void Shutdown() override
    {
        m_Status.initialized = false;
        m_Status.health = RenderBackendHealth::Shutdown;
    }

    void Resize(std::uint32_t width, std::uint32_t height) override
    {
        m_Width = width;
        m_Height = height;
    }

    TextureHandle CreateTexture(const TextureDesc&) override
    {
        return Next<TextureHandle>();
    }

    BufferHandle CreateBuffer(const BufferDesc&) override
    {
        return Next<BufferHandle>();
    }

    ShaderHandle CreateShader(const ShaderDesc&) override
    {
        return Next<ShaderHandle>();
    }

    PipelineHandle CreatePipeline(const PipelineDesc&) override
    {
        return Next<PipelineHandle>();
    }

    SamplerHandle CreateSampler(const SamplerDesc&) override
    {
        return Next<SamplerHandle>();
    }

    void DestroyTexture(TextureHandle) override {}
    void DestroyBuffer(BufferHandle) override {}
    void DestroyShader(ShaderHandle) override {}
    void DestroyPipeline(PipelineHandle) override {}
    void DestroySampler(SamplerHandle) override {}

    void BeginFrame() override
    {
        ++m_Status.frameIndex;
    }

    void EndFrame() override {}

    [[nodiscard]] RenderBackendStatus GetStatus() const noexcept override
    {
        return m_Status;
    }

    [[nodiscard]] RenderBackendCapabilities GetCapabilities()
        const noexcept override
    {
        RenderBackendCapabilities capabilities{};
        capabilities.backend = m_Status.selectedBackend;
        capabilities.qualityCeiling = RenderQualityPreset::Low;
        capabilities.maxTexture2DSize = 1024u;
        capabilities.maxColorAttachments = 1u;
        capabilities.maxFramesInFlight = 1u;
        return capabilities;
    }

    [[nodiscard]] std::uint32_t Width() const noexcept
    {
        return m_Width;
    }

    [[nodiscard]] std::uint32_t Height() const noexcept
    {
        return m_Height;
    }

private:
    template <typename HandleT>
    [[nodiscard]] HandleT Next() noexcept
    {
        HandleT handle;
        handle.index = m_NextIndex++;
        handle.generation = 1;
        return handle;
    }

    RenderBackendStatus m_Status{};
    std::uint32_t       m_Width = 0;
    std::uint32_t       m_Height = 0;
    std::uint32_t       m_NextIndex = 1;
};

} // namespace SagaEngine::Graphics
