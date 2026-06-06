/// @file GraphicsBackend.h
/// @brief Minimal vendor-neutral graphics backend shell.

#pragma once

#include "SagaEngine/Graphics/Core/GraphicsTypes.h"
#include "SagaEngine/Graphics/Descs/ResourceDescs.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

#include <cstddef>
#include <cstdint>
#include <vector>

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
    [[nodiscard]] virtual GraphicsResourceMemoryReport
    GetResourceMemoryReport() const noexcept = 0;
    [[nodiscard]] virtual GraphicsResourceFailure
    GetLastResourceFailure() const noexcept = 0;
    [[nodiscard]] virtual GraphicsResourceLeakSummary
    GetLastShutdownResourceLeakSummary() const noexcept = 0;
};

class NullGraphicsBackend final : public IGraphicsBackend
{
private:
    template <typename HandleT, typename DescT>
    class ResourceRegistry
    {
    public:
        [[nodiscard]] HandleT Create(
            const DescT& desc,
            std::uint64_t estimatedBytes,
            const std::vector<std::uint8_t>& shadowPayload = {})
        {
            std::uint32_t slotIndex = 0;
            if (!m_FreeSlots.empty())
            {
                slotIndex = m_FreeSlots.back();
                m_FreeSlots.pop_back();
                auto& slot = m_Slots[slotIndex];
                slot.desc = desc;
                slot.estimatedBytes = estimatedBytes;
                slot.shadowPayload = shadowPayload;
                slot.occupied = true;
                slot.generation = NextGeneration(slot.generation);
            }
            else
            {
                slotIndex = static_cast<std::uint32_t>(m_Slots.size());
                m_Slots.push_back(
                    {desc, estimatedBytes, shadowPayload, 1u, true});
            }

            m_LiveCount += 1u;
            m_LiveBytes += estimatedBytes;

            HandleT handle;
            handle.index = slotIndex + 1u;
            handle.generation = m_Slots[slotIndex].generation;
            return handle;
        }

        void Destroy(HandleT handle)
        {
            if (!handle.IsValid() || handle.index > m_Slots.size())
            {
                return;
            }

            const auto slotIndex = handle.index - 1u;
            auto& slot = m_Slots[slotIndex];
            if (!slot.occupied || slot.generation != handle.generation)
            {
                return;
            }

            slot.occupied = false;
            slot.shadowPayload.clear();
            m_LiveCount -= 1u;
            m_LiveBytes -= slot.estimatedBytes;
            m_FreeSlots.push_back(slotIndex);
        }

        void ReleaseAll()
        {
            m_FreeSlots.clear();
            for (std::uint32_t i = 0; i < m_Slots.size(); ++i)
            {
                auto& slot = m_Slots[i];
                slot.occupied = false;
                slot.shadowPayload.clear();
                m_FreeSlots.push_back(i);
            }
            m_LiveCount = 0;
            m_LiveBytes = 0;
        }

        [[nodiscard]] std::uint32_t LiveCount() const noexcept
        {
            return m_LiveCount;
        }

        [[nodiscard]] std::uint64_t LiveBytes() const noexcept
        {
            return m_LiveBytes;
        }

    private:
        struct Slot
        {
            DescT desc{};
            std::uint64_t estimatedBytes = 0;
            std::vector<std::uint8_t> shadowPayload{};
            std::uint32_t generation = 1;
            bool occupied = false;
        };

        [[nodiscard]] static constexpr std::uint32_t NextGeneration(
            std::uint32_t generation) noexcept
        {
            return generation == 0xFFFFFFFFu ? 1u : generation + 1u;
        }

        std::vector<Slot> m_Slots;
        std::vector<std::uint32_t> m_FreeSlots;
        std::uint32_t m_LiveCount = 0;
        std::uint64_t m_LiveBytes = 0;
    };

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
        m_LastShutdownLeakSummary =
            BuildLeakSummary(GetResourceMemoryReport());
        m_Textures.ReleaseAll();
        m_Buffers.ReleaseAll();
        m_Shaders.ReleaseAll();
        m_Pipelines.ReleaseAll();
        m_Samplers.ReleaseAll();
        m_Status.initialized = false;
        m_Status.health = RenderBackendHealth::Shutdown;
    }

    void Resize(std::uint32_t width, std::uint32_t height) override
    {
        m_Width = width;
        m_Height = height;
    }

    TextureHandle CreateTexture(const TextureDesc& desc) override
    {
        return CreateTexture(desc, {});
    }

    TextureHandle CreateTexture(
        const TextureDesc& desc,
        GraphicsDataView initialData) override
    {
        if (!CanCreateResources())
        {
            return RecordCreateFailure<TextureHandle>(
                GraphicsResourceFailure::BackendNotInitialized);
        }

        if (!IsValidTextureDesc(desc))
        {
            return RecordCreateFailure<TextureHandle>(
                GraphicsResourceFailure::InvalidTextureDesc);
        }

        if (!IsValidTextureInitialData(desc, initialData))
        {
            return RecordCreateFailure<TextureHandle>(
                GraphicsResourceFailure::InvalidInitialData);
        }

        return RecordSuccessfulCreate(
            m_Textures.Create(
                desc,
                EstimateTextureBytes(desc),
                CopyShadowPayload(initialData)));
    }

    BufferHandle CreateBuffer(const BufferDesc& desc) override
    {
        return CreateBuffer(desc, {});
    }

    BufferHandle CreateBuffer(
        const BufferDesc& desc,
        GraphicsDataView initialData) override
    {
        if (!CanCreateResources())
        {
            return RecordCreateFailure<BufferHandle>(
                GraphicsResourceFailure::BackendNotInitialized);
        }

        if (!IsValidBufferDesc(desc))
        {
            return RecordCreateFailure<BufferHandle>(
                GraphicsResourceFailure::InvalidBufferDesc);
        }

        if (!IsValidBufferInitialData(desc, initialData))
        {
            return RecordCreateFailure<BufferHandle>(
                GraphicsResourceFailure::InvalidInitialData);
        }

        return RecordSuccessfulCreate(
            m_Buffers.Create(
                desc,
                EstimateBufferBytes(desc),
                CopyShadowPayload(initialData)));
    }

    ShaderHandle CreateShader(const ShaderDesc& desc) override
    {
        if (!CanCreateResources())
        {
            return RecordCreateFailure<ShaderHandle>(
                GraphicsResourceFailure::BackendNotInitialized);
        }

        if (!IsValidShaderDesc(desc))
        {
            return RecordCreateFailure<ShaderHandle>(
                GraphicsResourceFailure::InvalidShaderDesc);
        }

        return RecordSuccessfulCreate(
            m_Shaders.Create(desc, EstimateShaderBytes(desc)));
    }

    PipelineHandle CreatePipeline(const PipelineDesc& desc) override
    {
        if (!CanCreateResources())
        {
            return RecordCreateFailure<PipelineHandle>(
                GraphicsResourceFailure::BackendNotInitialized);
        }

        if (!IsValidPipelineDesc(desc))
        {
            return RecordCreateFailure<PipelineHandle>(
                GraphicsResourceFailure::InvalidPipelineDesc);
        }

        return RecordSuccessfulCreate(
            m_Pipelines.Create(desc, EstimatePipelineBytes(desc)));
    }

    SamplerHandle CreateSampler(const SamplerDesc& desc) override
    {
        if (!CanCreateResources())
        {
            return RecordCreateFailure<SamplerHandle>(
                GraphicsResourceFailure::BackendNotInitialized);
        }

        if (!IsValidSamplerDesc(desc))
        {
            return RecordCreateFailure<SamplerHandle>(
                GraphicsResourceFailure::InvalidSamplerDesc);
        }

        return RecordSuccessfulCreate(
            m_Samplers.Create(desc, EstimateSamplerBytes(desc)));
    }

    void DestroyTexture(TextureHandle handle) override
    {
        m_Textures.Destroy(handle);
    }

    void DestroyBuffer(BufferHandle handle) override
    {
        m_Buffers.Destroy(handle);
    }

    void DestroyShader(ShaderHandle handle) override
    {
        m_Shaders.Destroy(handle);
    }

    void DestroyPipeline(PipelineHandle handle) override
    {
        m_Pipelines.Destroy(handle);
    }

    void DestroySampler(SamplerHandle handle) override
    {
        m_Samplers.Destroy(handle);
    }

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

    [[nodiscard]] GraphicsResourceMemoryReport GetResourceMemoryReport()
        const noexcept override
    {
        GraphicsResourceMemoryReport report{};
        report.liveTextureCount = m_Textures.LiveCount();
        report.liveBufferCount = m_Buffers.LiveCount();
        report.liveShaderCount = m_Shaders.LiveCount();
        report.livePipelineCount = m_Pipelines.LiveCount();
        report.liveSamplerCount = m_Samplers.LiveCount();
        report.textureBytes = m_Textures.LiveBytes();
        report.bufferBytes = m_Buffers.LiveBytes();
        report.shaderBytes = m_Shaders.LiveBytes();
        report.pipelineBytes = m_Pipelines.LiveBytes();
        report.samplerBytes = m_Samplers.LiveBytes();
        report.totalLiveBytes =
            report.textureBytes + report.bufferBytes + report.shaderBytes +
            report.pipelineBytes + report.samplerBytes;
        report.peakLiveBytes = m_PeakLiveBytes;
        report.failedCreateCount = m_FailedCreateCount;
        return report;
    }

    [[nodiscard]] GraphicsResourceFailure GetLastResourceFailure()
        const noexcept override
    {
        return m_LastResourceFailure;
    }

    [[nodiscard]] GraphicsResourceLeakSummary
    GetLastShutdownResourceLeakSummary() const noexcept override
    {
        return m_LastShutdownLeakSummary;
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
    [[nodiscard]] bool CanCreateResources() const noexcept
    {
        return m_Status.initialized &&
               m_Status.failure == RenderBackendFailure::None;
    }

    template <typename HandleT>
    [[nodiscard]] HandleT RecordCreateFailure(
        GraphicsResourceFailure failure) noexcept
    {
        m_LastResourceFailure = failure;
        ++m_FailedCreateCount;
        return {};
    }

    template <typename HandleT>
    [[nodiscard]] HandleT RecordSuccessfulCreate(HandleT handle) noexcept
    {
        m_LastResourceFailure = GraphicsResourceFailure::None;
        const auto report = GetResourceMemoryReport();
        if (report.totalLiveBytes > m_PeakLiveBytes)
        {
            m_PeakLiveBytes = report.totalLiveBytes;
        }
        return handle;
    }

    [[nodiscard]] static constexpr GraphicsResourceLeakSummary
    BuildLeakSummary(const GraphicsResourceMemoryReport& report) noexcept
    {
        GraphicsResourceLeakSummary summary{};
        summary.hadLiveResources = report.totalLiveBytes != 0u ||
                                   report.liveTextureCount != 0u ||
                                   report.liveBufferCount != 0u ||
                                   report.liveShaderCount != 0u ||
                                   report.livePipelineCount != 0u ||
                                   report.liveSamplerCount != 0u;
        summary.leakedTextureCount = report.liveTextureCount;
        summary.leakedBufferCount = report.liveBufferCount;
        summary.leakedShaderCount = report.liveShaderCount;
        summary.leakedPipelineCount = report.livePipelineCount;
        summary.leakedSamplerCount = report.liveSamplerCount;
        summary.textureBytes = report.textureBytes;
        summary.bufferBytes = report.bufferBytes;
        summary.shaderBytes = report.shaderBytes;
        summary.pipelineBytes = report.pipelineBytes;
        summary.samplerBytes = report.samplerBytes;
        summary.totalLeakedBytes = report.totalLiveBytes;
        return summary;
    }

    [[nodiscard]] static constexpr bool IsKnownFormat(
        ResourceFormat format) noexcept
    {
        return format == ResourceFormat::Rgba8Unorm ||
               format == ResourceFormat::Bgra8Unorm ||
               format == ResourceFormat::Rgba16Float ||
               format == ResourceFormat::Rgba32Float ||
               format == ResourceFormat::Depth24Stencil8 ||
               format == ResourceFormat::Depth32Float;
    }

    [[nodiscard]] static constexpr bool IsValidTextureDesc(
        const TextureDesc& desc) noexcept
    {
        return desc.width != 0u && desc.height != 0u && desc.depth != 0u &&
               desc.mipLevels != 0u && desc.arrayLayers != 0u &&
               IsKnownFormat(desc.format);
    }

    [[nodiscard]] static constexpr bool IsValidBufferDesc(
        const BufferDesc& desc) noexcept
    {
        return desc.sizeBytes != 0u;
    }

    [[nodiscard]] static constexpr bool IsValidShaderStage(
        ShaderStage stage) noexcept
    {
        return stage == ShaderStage::Vertex ||
               stage == ShaderStage::Fragment ||
               stage == ShaderStage::Compute;
    }

    [[nodiscard]] static constexpr bool IsValidShaderDesc(
        const ShaderDesc& desc) noexcept
    {
        return desc.byteSize != 0u && IsValidShaderStage(desc.stage);
    }

    [[nodiscard]] static constexpr bool IsValidTopology(
        PrimitiveTopology topology) noexcept
    {
        return topology == PrimitiveTopology::TriangleList ||
               topology == PrimitiveTopology::TriangleStrip ||
               topology == PrimitiveTopology::LineList;
    }

    [[nodiscard]] static constexpr bool IsValidPipelineDesc(
        const PipelineDesc& desc) noexcept
    {
        return desc.colorTargetCount != 0u && desc.colorTargetCount <= 8u &&
               IsValidTopology(desc.topology) &&
               IsKnownFormat(desc.colorFormat) &&
               IsKnownFormat(desc.depthFormat);
    }

    [[nodiscard]] static constexpr bool IsValidFilterMode(
        FilterMode mode) noexcept
    {
        return mode == FilterMode::Nearest || mode == FilterMode::Linear;
    }

    [[nodiscard]] static constexpr bool IsValidAddressMode(
        AddressMode mode) noexcept
    {
        return mode == AddressMode::ClampToEdge ||
               mode == AddressMode::Repeat ||
               mode == AddressMode::MirrorRepeat;
    }

    [[nodiscard]] static constexpr bool IsValidSamplerDesc(
        const SamplerDesc& desc) noexcept
    {
        return IsValidFilterMode(desc.minFilter) &&
               IsValidFilterMode(desc.magFilter) &&
               IsValidAddressMode(desc.addressU) &&
               IsValidAddressMode(desc.addressV) &&
               IsValidAddressMode(desc.addressW);
    }

    [[nodiscard]] static constexpr std::uint64_t BytesPerPixel(
        ResourceFormat format) noexcept
    {
        switch (format)
        {
        case ResourceFormat::Rgba8Unorm:
        case ResourceFormat::Bgra8Unorm:
        case ResourceFormat::Depth24Stencil8:
        case ResourceFormat::Depth32Float:
            return 4u;
        case ResourceFormat::Rgba16Float:
            return 8u;
        case ResourceFormat::Rgba32Float:
            return 16u;
        case ResourceFormat::Unknown:
        default:
            return 0u;
        }
    }

    [[nodiscard]] static constexpr std::uint64_t EstimateTextureBytes(
        const TextureDesc& desc) noexcept
    {
        std::uint64_t totalPixels = 0;
        std::uint64_t width = desc.width;
        std::uint64_t height = desc.height;
        std::uint64_t depth = desc.depth;

        for (std::uint16_t mip = 0; mip < desc.mipLevels; ++mip)
        {
            totalPixels += width * height * depth * desc.arrayLayers;
            width = width > 1u ? width / 2u : 1u;
            height = height > 1u ? height / 2u : 1u;
            depth = depth > 1u ? depth / 2u : 1u;
        }

        return totalPixels * BytesPerPixel(desc.format);
    }

    [[nodiscard]] static constexpr bool IsValidTextureInitialData(
        const TextureDesc& desc,
        GraphicsDataView initialData) noexcept
    {
        if (initialData.sizeBytes == 0u)
        {
            return true;
        }

        if (!initialData.data)
        {
            return false;
        }

        const auto bytesPerPixel = BytesPerPixel(desc.format);
        if (bytesPerPixel == 0u)
        {
            return false;
        }

        const auto rowBytes =
            static_cast<std::uint64_t>(desc.width) * bytesPerPixel;
        if (initialData.rowPitchBytes != 0u &&
            initialData.rowPitchBytes < rowBytes)
        {
            return false;
        }

        const auto effectiveRowPitch =
            initialData.rowPitchBytes == 0u
                ? rowBytes
                : static_cast<std::uint64_t>(initialData.rowPitchBytes);
        const auto sliceBytes =
            effectiveRowPitch * static_cast<std::uint64_t>(desc.height);
        if (initialData.slicePitchBytes != 0u &&
            initialData.slicePitchBytes < sliceBytes)
        {
            return false;
        }

        const auto effectiveSlicePitch =
            initialData.slicePitchBytes == 0u
                ? sliceBytes
                : static_cast<std::uint64_t>(initialData.slicePitchBytes);
        const auto requiredBytes =
            effectiveSlicePitch * static_cast<std::uint64_t>(desc.depth);

        return initialData.sizeBytes >= requiredBytes &&
               initialData.sizeBytes <= EstimateTextureBytes(desc);
    }

    [[nodiscard]] static constexpr std::uint64_t EstimateBufferBytes(
        const BufferDesc& desc) noexcept
    {
        return desc.sizeBytes;
    }

    [[nodiscard]] static constexpr bool IsValidBufferInitialData(
        const BufferDesc& desc,
        GraphicsDataView initialData) noexcept
    {
        if (initialData.sizeBytes == 0u)
        {
            return true;
        }

        return initialData.data && initialData.sizeBytes <= desc.sizeBytes;
    }

    [[nodiscard]] static std::vector<std::uint8_t> CopyShadowPayload(
        GraphicsDataView initialData)
    {
        std::vector<std::uint8_t> payload;
        if (!initialData.data || initialData.sizeBytes == 0u)
        {
            return payload;
        }

        const auto* bytes =
            static_cast<const std::uint8_t*>(initialData.data);
        payload.assign(
            bytes,
            bytes + static_cast<std::size_t>(initialData.sizeBytes));
        return payload;
    }

    [[nodiscard]] static constexpr std::uint64_t EstimateShaderBytes(
        const ShaderDesc& desc) noexcept
    {
        return desc.byteSize;
    }

    [[nodiscard]] static constexpr std::uint64_t EstimatePipelineBytes(
        const PipelineDesc&) noexcept
    {
        return 0u;
    }

    [[nodiscard]] static constexpr std::uint64_t EstimateSamplerBytes(
        const SamplerDesc&) noexcept
    {
        return 0u;
    }

    RenderBackendStatus m_Status{};
    std::uint32_t       m_Width = 0;
    std::uint32_t       m_Height = 0;
    GraphicsResourceFailure m_LastResourceFailure =
        GraphicsResourceFailure::None;
    GraphicsResourceLeakSummary m_LastShutdownLeakSummary{};
    std::uint64_t m_PeakLiveBytes = 0;
    std::uint64_t m_FailedCreateCount = 0;
    ResourceRegistry<TextureHandle, TextureDesc> m_Textures;
    ResourceRegistry<BufferHandle, BufferDesc> m_Buffers;
    ResourceRegistry<ShaderHandle, ShaderDesc> m_Shaders;
    ResourceRegistry<PipelineHandle, PipelineDesc> m_Pipelines;
    ResourceRegistry<SamplerHandle, SamplerDesc> m_Samplers;
};

} // namespace SagaEngine::Graphics
