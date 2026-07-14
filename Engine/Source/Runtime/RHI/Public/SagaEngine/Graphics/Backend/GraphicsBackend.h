/// @file GraphicsBackend.h
/// @brief Minimal vendor-neutral graphics backend shell.

#pragma once

#include "SagaEngine/Graphics/Core/GraphicsTypes.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingTypes.h"
#include "SagaEngine/Graphics/Descs/ResourceDescs.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

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

enum class GraphicsResourceFailure : std::uint8_t
{
    None = 0,
    BackendNotInitialized,
    InvalidTextureDesc,
    InvalidBufferDesc,
    InvalidShaderDesc,
    InvalidPipelineDesc,
    InvalidSamplerDesc,
    InvalidInitialData,
    InvalidBindingLayoutDesc,
    InvalidBindingSetDesc,
    UnsupportedBindingContract,
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

struct GraphicsResourceMemoryReport
{
    std::uint32_t liveTextureCount = 0;
    std::uint32_t liveBufferCount = 0;
    std::uint32_t liveShaderCount = 0;
    std::uint32_t livePipelineCount = 0;
    std::uint32_t liveSamplerCount = 0;
    std::uint64_t textureBytes = 0;
    std::uint64_t bufferBytes = 0;
    std::uint64_t shaderBytes = 0;
    std::uint64_t pipelineBytes = 0;
    std::uint64_t samplerBytes = 0;
    std::uint64_t totalLiveBytes = 0;
    std::uint64_t peakLiveBytes = 0;
    std::uint64_t failedCreateCount = 0;
    std::uint32_t pendingDestroyTextureCount = 0;
    std::uint32_t pendingDestroyBufferCount = 0;
    std::uint32_t pendingDestroyShaderCount = 0;
    std::uint32_t pendingDestroyPipelineCount = 0;
    std::uint32_t pendingDestroySamplerCount = 0;
    std::uint64_t pendingDestroyBytes = 0;
    std::uint64_t uploadReservedBytes = 0;
    std::uint64_t uploadUsedBytes = 0;
    std::uint64_t uploadInFlightBytes = 0;
    std::uint64_t peakUploadReservedBytes = 0;
    std::uint64_t peakUploadUsedBytes = 0;
    std::uint64_t nativeCommittedBytes = 0;
    bool nativeCommittedBytesKnown = false;
    bool overSoftBudget = false;
    std::uint64_t softBudgetBytes = 0;
};

struct GraphicsResourceLeakSummary
{
    bool hadLiveResources = false;
    std::uint32_t leakedTextureCount = 0;
    std::uint32_t leakedBufferCount = 0;
    std::uint32_t leakedShaderCount = 0;
    std::uint32_t leakedPipelineCount = 0;
    std::uint32_t leakedSamplerCount = 0;
    std::uint64_t textureBytes = 0;
    std::uint64_t bufferBytes = 0;
    std::uint64_t shaderBytes = 0;
    std::uint64_t pipelineBytes = 0;
    std::uint64_t samplerBytes = 0;
    std::uint64_t totalLeakedBytes = 0;
};

/// Creation-time data view. The pointer only needs to remain valid for the
/// duration of the create call; backends copy accepted data immediately and
/// never retain the caller-owned pointer.
struct GraphicsDataView
{
    const void*   data = nullptr;
    std::uint64_t sizeBytes = 0;
    std::uint32_t rowPitchBytes = 0;
    std::uint32_t slicePitchBytes = 0;
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
    [[nodiscard]] virtual TextureHandle CreateTexture(
        const TextureDesc& desc,
        GraphicsDataView initialData) = 0;
    [[nodiscard]] virtual BufferHandle CreateBuffer(
        const BufferDesc& desc) = 0;
    [[nodiscard]] virtual BufferHandle CreateBuffer(
        const BufferDesc& desc,
        GraphicsDataView initialData) = 0;
    [[nodiscard]] virtual ShaderHandle CreateShader(
        const ShaderDesc& desc) = 0;
    [[nodiscard]] virtual PipelineHandle CreatePipeline(
        const PipelineDesc& desc) = 0;
    [[nodiscard]] virtual SamplerHandle CreateSampler(
        const SamplerDesc& desc) = 0;
    [[nodiscard]] virtual BindingLayoutHandle CreateBindingLayout(
        const GraphicsBindingLayoutDesc&)
    {
        return {};
    }
    [[nodiscard]] virtual BindingSetHandle CreateBindingSet(
        const GraphicsBindingSetDesc&)
    {
        return {};
    }

    virtual void DestroyTexture(TextureHandle handle) = 0;
    virtual void DestroyBuffer(BufferHandle handle) = 0;
    virtual void DestroyShader(ShaderHandle handle) = 0;
    virtual void DestroyPipeline(PipelineHandle handle) = 0;
    virtual void DestroySampler(SamplerHandle handle) = 0;
    virtual void DestroyBindingLayout(BindingLayoutHandle) {}
    virtual void DestroyBindingSet(BindingSetHandle) {}

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    [[nodiscard]] virtual RenderBackendStatus GetStatus() const noexcept = 0;
    [[nodiscard]] virtual RenderBackendCapabilities
    GetCapabilities() const noexcept = 0;
    [[nodiscard]] virtual GraphicsResourceMemoryReport
    GetResourceMemoryReport() const noexcept = 0;
    [[nodiscard]] virtual GraphicsResourceFailure
    GetLastResourceFailure() const noexcept = 0;
    [[nodiscard]] virtual GraphicsResourceLeakSummary
    GetLastShutdownResourceLeakSummary() const noexcept = 0;
    [[nodiscard]] virtual GraphicsResourceQueryResult QueryResource(
        GraphicsResourceKind kind,
        GraphicsHandle handle) const = 0;
    [[nodiscard]] virtual GraphicsBindingLayoutQueryResult QueryBindingLayout(
        BindingLayoutHandle) const
    {
        return {};
    }
    [[nodiscard]] virtual GraphicsBindingSetQueryResult QueryBindingSet(
        BindingSetHandle) const
    {
        return {};
    }
};

/// Vendor-neutral no-op graphics backend. It is noncopyable and noexcept
/// movable. A moved-from instance is valid but inert: Initialize fails,
/// Shutdown and destroy operations are no-op, and create operations return
/// invalid handles.
class NullGraphicsBackend final : public IGraphicsBackend
{
public:
    NullGraphicsBackend();
    ~NullGraphicsBackend() override;
    NullGraphicsBackend(const NullGraphicsBackend&) = delete;
    NullGraphicsBackend& operator=(const NullGraphicsBackend&) = delete;
    NullGraphicsBackend(NullGraphicsBackend&&) noexcept;
    NullGraphicsBackend& operator=(NullGraphicsBackend&&) noexcept;

    bool Initialize(
        const RenderBackendDesc& backend,
        const SwapchainDesc& swapchain) override;
    void Shutdown() override;
    void Resize(std::uint32_t width, std::uint32_t height) override;

    [[nodiscard]] TextureHandle CreateTexture(const TextureDesc& desc) override;
    [[nodiscard]] TextureHandle CreateTexture(
        const TextureDesc& desc,
        GraphicsDataView initialData) override;
    [[nodiscard]] BufferHandle CreateBuffer(const BufferDesc& desc) override;
    [[nodiscard]] BufferHandle CreateBuffer(
        const BufferDesc& desc,
        GraphicsDataView initialData) override;
    [[nodiscard]] ShaderHandle CreateShader(const ShaderDesc& desc) override;
    [[nodiscard]] PipelineHandle CreatePipeline(const PipelineDesc& desc) override;
    [[nodiscard]] SamplerHandle CreateSampler(const SamplerDesc& desc) override;
    [[nodiscard]] BindingLayoutHandle CreateBindingLayout(
        const GraphicsBindingLayoutDesc& desc) override;
    [[nodiscard]] BindingSetHandle CreateBindingSet(
        const GraphicsBindingSetDesc& desc) override;

    void DestroyTexture(TextureHandle handle) override;
    void DestroyBuffer(BufferHandle handle) override;
    void DestroyShader(ShaderHandle handle) override;
    void DestroyPipeline(PipelineHandle handle) override;
    void DestroySampler(SamplerHandle handle) override;
    void DestroyBindingLayout(BindingLayoutHandle handle) override;
    void DestroyBindingSet(BindingSetHandle handle) override;

    void BeginFrame() override;
    void EndFrame() override;

    [[nodiscard]] RenderBackendStatus GetStatus() const noexcept override;
    [[nodiscard]] RenderBackendCapabilities GetCapabilities()
        const noexcept override;
    [[nodiscard]] GraphicsResourceMemoryReport GetResourceMemoryReport()
        const noexcept override;
    [[nodiscard]] GraphicsResourceFailure GetLastResourceFailure()
        const noexcept override;
    [[nodiscard]] GraphicsResourceLeakSummary
    GetLastShutdownResourceLeakSummary() const noexcept override;
    [[nodiscard]] GraphicsResourceQueryResult QueryResource(
        GraphicsResourceKind kind,
        GraphicsHandle handle) const override;
    [[nodiscard]] GraphicsBindingLayoutQueryResult QueryBindingLayout(
        BindingLayoutHandle handle) const override;
    [[nodiscard]] GraphicsBindingSetQueryResult QueryBindingSet(
        BindingSetHandle handle) const override;

    [[nodiscard]] GraphicsResourceBacking GetTextureBackingForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceBacking GetBufferBackingForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QueryTextureForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QueryBufferForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QueryShaderForTesting(
        ShaderHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QueryPipelineForTesting(
        PipelineHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QuerySamplerForTesting(
        SamplerHandle handle) const noexcept;
    [[nodiscard]] std::uint32_t Width() const noexcept;
    [[nodiscard]] std::uint32_t Height() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace SagaEngine::Graphics
