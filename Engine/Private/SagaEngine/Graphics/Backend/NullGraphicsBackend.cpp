/// @file NullGraphicsBackend.cpp
/// @brief Null graphics backend implementation.

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"

#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace SagaEngine::Graphics
{

class NullGraphicsBackend::Impl final
{
private:
    template <typename HandleT, typename DescT>
    class ResourceRegistry
    {
    public:
        explicit ResourceRegistry(std::uint32_t initialGeneration = 1u)
            : m_InitialGeneration(initialGeneration == 0u ? 1u
                                                          : initialGeneration)
        {
        }

        [[nodiscard]] HandleT Create(
            const DescT& desc,
            std::uint64_t estimatedBytes,
            const std::vector<std::uint8_t>& shadowPayload = {},
            std::uint64_t creationSerial = 0)
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
                slot.backing = GraphicsResourceBacking::RegisteredOnly;
                slot.lifecycle = GraphicsResourceLifecycle::RegisteredOnly;
                slot.creationSerial = creationSerial;
                slot.lastUseSerial = 0;
                slot.occupied = true;
                slot.generation = NextGeneration(slot.generation);
            }
            else
            {
                slotIndex = static_cast<std::uint32_t>(m_Slots.size());
                m_Slots.push_back(
                    {
                        desc,
                        estimatedBytes,
                        shadowPayload,
                        GraphicsResourceBacking::RegisteredOnly,
                        GraphicsResourceLifecycle::RegisteredOnly,
                        creationSerial,
                        0u,
                        m_InitialGeneration,
                        true,
                    });
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
            slot.lifecycle = GraphicsResourceLifecycle::Retired;
            slot.lastUseSerial = slot.creationSerial;
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
                slot.lifecycle = GraphicsResourceLifecycle::Retired;
                slot.lastUseSerial = slot.creationSerial;
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

        [[nodiscard]] GraphicsResourceBacking Backing(HandleT handle)
            const noexcept
        {
            if (!handle.IsValid() || handle.index > m_Slots.size())
            {
                return GraphicsResourceBacking::Invalid;
            }

            const auto slotIndex = handle.index - 1u;
            const auto& slot = m_Slots[slotIndex];
            if (!slot.occupied || slot.generation != handle.generation)
            {
                return GraphicsResourceBacking::Invalid;
            }

            return slot.backing;
        }

        [[nodiscard]] GraphicsResourceQueryResult Query(
            HandleT handle,
            GraphicsResourceKind kind) const
        {
            if (!handle.IsValid() || handle.index > m_Slots.size())
            {
                return {};
            }

            const auto slotIndex = handle.index - 1u;
            const auto& slot = m_Slots[slotIndex];
            if (!slot.occupied || slot.generation != handle.generation)
            {
                return {};
            }

            return {
                true,
                true,
                kind,
                slot.lifecycle,
                slot.backing,
                slot.backing == GraphicsResourceBacking::NativeGpu,
                slot.estimatedBytes,
                slot.estimatedBytes,
                slot.lifecycle == GraphicsResourceLifecycle::Ready &&
                    slot.backing == GraphicsResourceBacking::NativeGpu,
                slot.lifecycle == GraphicsResourceLifecycle::PendingDestroy,
                slot.creationSerial,
                slot.lastUseSerial,
                slot.desc.debugName,
            };
        }

        [[nodiscard]] const DescT* ResolveDesc(HandleT handle) const noexcept
        {
            if (!handle.IsValid() || handle.index > m_Slots.size())
            {
                return nullptr;
            }

            const auto slotIndex = handle.index - 1u;
            const auto& slot = m_Slots[slotIndex];
            if (!slot.occupied || slot.generation != handle.generation)
            {
                return nullptr;
            }

            return &slot.desc;
        }

    private:
        struct Slot
        {
            DescT desc{};
            std::uint64_t estimatedBytes = 0;
            std::vector<std::uint8_t> shadowPayload{};
            GraphicsResourceBacking backing =
                GraphicsResourceBacking::RegisteredOnly;
            GraphicsResourceLifecycle lifecycle =
                GraphicsResourceLifecycle::Invalid;
            std::uint64_t creationSerial = 0;
            std::uint64_t lastUseSerial = 0;
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
        std::uint32_t m_InitialGeneration = 1;
        std::uint32_t m_LiveCount = 0;
        std::uint64_t m_LiveBytes = 0;
    };

public:
    bool Initialize(
        const RenderBackendDesc& backend,
        const SwapchainDesc&)
    {
        m_Status.selectedBackend = backend.preferredBackend;
        m_Status.initialized = true;
        m_Status.health =
            backend.headless ? RenderBackendHealth::Headless
                             : RenderBackendHealth::Ready;
        m_Status.failure = RenderBackendFailure::None;
        return true;
    }

    void Shutdown()
    {
        m_LastShutdownLeakSummary =
            BuildLeakSummary(GetResourceMemoryReport());
        m_Textures.ReleaseAll();
        m_Buffers.ReleaseAll();
        m_Shaders.ReleaseAll();
        m_Pipelines.ReleaseAll();
        m_Samplers.ReleaseAll();
        m_BindingSets.ReleaseAll();
        m_BindingLayouts.ReleaseAll();
        m_Status.initialized = false;
        m_Status.health = RenderBackendHealth::Shutdown;
    }

    void Resize(std::uint32_t width, std::uint32_t height)
    {
        m_Width = width;
        m_Height = height;
    }

    TextureHandle CreateTexture(const TextureDesc& desc)
    {
        return CreateTexture(desc, {});
    }

    TextureHandle CreateTexture(
        const TextureDesc& desc,
        GraphicsDataView initialData)
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
                CopyShadowPayload(initialData),
                m_NextResourceCreationSerial++));
    }

    BufferHandle CreateBuffer(const BufferDesc& desc)
    {
        return CreateBuffer(desc, {});
    }

    BufferHandle CreateBuffer(
        const BufferDesc& desc,
        GraphicsDataView initialData)
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
                CopyShadowPayload(initialData),
                m_NextResourceCreationSerial++));
    }

    ShaderHandle CreateShader(const ShaderDesc& desc)
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
            m_Shaders.Create(
                desc,
                EstimateShaderBytes(desc),
                {},
                m_NextResourceCreationSerial++));
    }

    PipelineHandle CreatePipeline(const PipelineDesc& desc)
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

        PipelineDesc storedDesc = desc;
        if (desc.bindingLayout.IsValid())
        {
            const auto layout = QueryBindingLayout(desc.bindingLayout);
            if (!layout.live)
            {
                return RecordCreateFailure<PipelineHandle>(
                    GraphicsResourceFailure::InvalidPipelineDesc);
            }

            if (desc.bindingCompatibilityKey != 0u &&
                desc.bindingCompatibilityKey != layout.compatibilityKey)
            {
                return RecordCreateFailure<PipelineHandle>(
                    GraphicsResourceFailure::InvalidPipelineDesc);
            }

            if (!desc.bindingCompatibilityLayout.slots.empty())
            {
                if (!ValidateGraphicsBindingLayout(
                         desc.bindingCompatibilityLayout)
                         .valid)
                {
                    return RecordCreateFailure<PipelineHandle>(
                        GraphicsResourceFailure::InvalidPipelineDesc);
                }

                const auto requestedKey =
                    desc.bindingCompatibilityKey != 0u
                        ? desc.bindingCompatibilityKey
                        : ComputeGraphicsBindingLayoutKey(
                              desc.bindingCompatibilityLayout);
                if (!AreGraphicsBindingLayoutsCompatible(
                        desc.bindingCompatibilityLayout,
                        requestedKey,
                        layout.canonicalLayout,
                        layout.compatibilityKey))
                {
                    return RecordCreateFailure<PipelineHandle>(
                        GraphicsResourceFailure::InvalidPipelineDesc);
                }
            }

            storedDesc.bindingCompatibilityKey = layout.compatibilityKey;
            storedDesc.bindingCompatibilityLayout = layout.canonicalLayout;
        }

        return RecordSuccessfulCreate(
            m_Pipelines.Create(
                storedDesc,
                EstimatePipelineBytes(storedDesc),
                {},
                m_NextResourceCreationSerial++));
    }

    SamplerHandle CreateSampler(const SamplerDesc& desc)
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
            m_Samplers.Create(
                desc,
                EstimateSamplerBytes(desc),
                {},
                m_NextResourceCreationSerial++));
    }

    BindingLayoutHandle CreateBindingLayout(
        const GraphicsBindingLayoutDesc& desc)
    {
        if (!CanCreateResources())
        {
            return RecordCreateFailure<BindingLayoutHandle>(
                GraphicsResourceFailure::BackendNotInitialized);
        }

        if (!ValidateGraphicsBindingLayout(desc).valid)
        {
            return RecordCreateFailure<BindingLayoutHandle>(
                GraphicsResourceFailure::InvalidBindingLayoutDesc);
        }

        const auto normalized = NormalizeGraphicsBindingLayout(desc);
        return RecordSuccessfulCreate(
            m_BindingLayouts.Create(
                normalized,
                EstimateBindingLayoutBytes(normalized),
                {},
                m_NextResourceCreationSerial++));
    }

    BindingSetHandle CreateBindingSet(
        const GraphicsBindingSetDesc& desc)
    {
        if (!CanCreateResources())
        {
            return RecordCreateFailure<BindingSetHandle>(
                GraphicsResourceFailure::BackendNotInitialized);
        }

        const auto* layoutDesc = m_BindingLayouts.ResolveDesc(desc.layout);
        if (!layoutDesc)
        {
            return RecordCreateFailure<BindingSetHandle>(
                GraphicsResourceFailure::InvalidBindingSetDesc);
        }

        if (!ValidateGraphicsBindingSet(
                 *layoutDesc,
                 desc,
                 QueryNullBindingResource,
                 this)
                 .valid)
        {
            return RecordCreateFailure<BindingSetHandle>(
                GraphicsResourceFailure::InvalidBindingSetDesc);
        }

        return RecordSuccessfulCreate(
            m_BindingSets.Create(
                desc,
                EstimateBindingSetBytes(desc),
                {},
                m_NextResourceCreationSerial++));
    }

    void DestroyTexture(TextureHandle handle)
    {
        m_Textures.Destroy(handle);
    }

    void DestroyBuffer(BufferHandle handle)
    {
        m_Buffers.Destroy(handle);
    }

    void DestroyShader(ShaderHandle handle)
    {
        m_Shaders.Destroy(handle);
    }

    void DestroyPipeline(PipelineHandle handle)
    {
        m_Pipelines.Destroy(handle);
    }

    void DestroySampler(SamplerHandle handle)
    {
        m_Samplers.Destroy(handle);
    }

    void DestroyBindingLayout(BindingLayoutHandle handle)
    {
        m_BindingLayouts.Destroy(handle);
    }

    void DestroyBindingSet(BindingSetHandle handle)
    {
        m_BindingSets.Destroy(handle);
    }

    void BeginFrame()
    {
        ++m_Status.frameIndex;
    }

    void EndFrame() {}

    [[nodiscard]] RenderBackendStatus GetStatus() const noexcept
    {
        return m_Status;
    }

    [[nodiscard]] RenderBackendCapabilities GetCapabilities()
        const noexcept
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
        const noexcept
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
        const noexcept
    {
        return m_LastResourceFailure;
    }

    [[nodiscard]] GraphicsResourceLeakSummary
    GetLastShutdownResourceLeakSummary() const noexcept
    {
        return m_LastShutdownLeakSummary;
    }

    [[nodiscard]] GraphicsResourceQueryResult QueryResource(
        GraphicsResourceKind kind,
        GraphicsHandle handle) const
    {
        switch (kind)
        {
        case GraphicsResourceKind::Texture:
            return QueryTextureForTesting(ToTypedHandle<TextureHandle>(handle));
        case GraphicsResourceKind::Buffer:
            return QueryBufferForTesting(ToTypedHandle<BufferHandle>(handle));
        case GraphicsResourceKind::Shader:
            return QueryShaderForTesting(ToTypedHandle<ShaderHandle>(handle));
        case GraphicsResourceKind::Pipeline:
            return QueryPipelineForTesting(
                ToTypedHandle<PipelineHandle>(handle));
        case GraphicsResourceKind::Sampler:
            return QuerySamplerForTesting(ToTypedHandle<SamplerHandle>(handle));
        case GraphicsResourceKind::Invalid:
        default:
            return {};
        }
    }

    [[nodiscard]] GraphicsResourceBacking GetTextureBackingForTesting(
        TextureHandle handle) const noexcept
    {
        return m_Textures.Backing(handle);
    }

    [[nodiscard]] GraphicsResourceBacking GetBufferBackingForTesting(
        BufferHandle handle) const noexcept
    {
        return m_Buffers.Backing(handle);
    }

    [[nodiscard]] GraphicsResourceQueryResult QueryTextureForTesting(
        TextureHandle handle) const noexcept
    {
        return m_Textures.Query(handle, GraphicsResourceKind::Texture);
    }

    [[nodiscard]] GraphicsResourceQueryResult QueryBufferForTesting(
        BufferHandle handle) const noexcept
    {
        return m_Buffers.Query(handle, GraphicsResourceKind::Buffer);
    }

    [[nodiscard]] GraphicsResourceQueryResult QueryShaderForTesting(
        ShaderHandle handle) const noexcept
    {
        return m_Shaders.Query(handle, GraphicsResourceKind::Shader);
    }

    [[nodiscard]] GraphicsResourceQueryResult QueryPipelineForTesting(
        PipelineHandle handle) const noexcept
    {
        return m_Pipelines.Query(handle, GraphicsResourceKind::Pipeline);
    }

    [[nodiscard]] GraphicsResourceQueryResult QuerySamplerForTesting(
        SamplerHandle handle) const noexcept
    {
        return m_Samplers.Query(handle, GraphicsResourceKind::Sampler);
    }

    [[nodiscard]] GraphicsBindingLayoutQueryResult QueryBindingLayout(
        BindingLayoutHandle handle) const
    {
        const auto* desc = m_BindingLayouts.ResolveDesc(handle);
        if (!desc)
        {
            return {};
        }

        return {
            true,
            true,
            ComputeGraphicsBindingLayoutKey(*desc),
            static_cast<std::uint32_t>(desc->slots.size()),
            desc->debugName,
            *desc,
        };
    }

    [[nodiscard]] GraphicsBindingSetQueryResult QueryBindingSet(
        BindingSetHandle handle) const
    {
        const auto* desc = m_BindingSets.ResolveDesc(handle);
        if (!desc)
        {
            return {};
        }

        const auto* layoutDesc = m_BindingLayouts.ResolveDesc(desc->layout);
        const auto layoutQuery = QueryBindingLayout(desc->layout);
        return {
            true,
            true,
            desc->layout,
            layoutQuery.compatibilityKey,
            static_cast<std::uint32_t>(desc->resources.size()),
            layoutDesc ? CountGraphicsBindingFallbackRequirements(
                             *layoutDesc,
                             *desc)
                       : 0u,
            desc->debugName,
        };
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
    [[nodiscard]] static constexpr HandleT ToTypedHandle(
        GraphicsHandle handle) noexcept
    {
        HandleT typed{};
        typed.index = handle.index;
        typed.generation = handle.generation;
        return typed;
    }

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
               format == ResourceFormat::Rgba8UnormSrgb ||
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
        return (desc.byteSize != 0u || !desc.source.empty()) &&
               IsValidShaderStage(desc.stage) &&
               (desc.source.empty() || !desc.entryPoint.empty());
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
        case ResourceFormat::Rgba8UnormSrgb:
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
        return desc.source.empty()
                   ? desc.byteSize
                   : static_cast<std::uint64_t>(desc.source.size());
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

    [[nodiscard]] static constexpr std::uint64_t EstimateBindingLayoutBytes(
        const GraphicsBindingLayoutDesc& desc) noexcept
    {
        return static_cast<std::uint64_t>(desc.slots.size()) *
               sizeof(GraphicsBindingLayoutSlot);
    }

    [[nodiscard]] static constexpr std::uint64_t EstimateBindingSetBytes(
        const GraphicsBindingSetDesc& desc) noexcept
    {
        return static_cast<std::uint64_t>(desc.resources.size()) *
               sizeof(GraphicsBindingResourceRef);
    }

    [[nodiscard]] static GraphicsResourceQueryResult QueryNullBindingResource(
        void* userData,
        GraphicsResourceKind kind,
        GraphicsHandle handle)
    {
        return static_cast<NullGraphicsBackend::Impl*>(userData)->QueryResource(
            kind,
            handle);
    }

    RenderBackendStatus m_Status{};
    std::uint32_t       m_Width = 0;
    std::uint32_t       m_Height = 0;
    GraphicsResourceFailure m_LastResourceFailure =
        GraphicsResourceFailure::None;
    GraphicsResourceLeakSummary m_LastShutdownLeakSummary{};
    std::uint64_t m_PeakLiveBytes = 0;
    std::uint64_t m_FailedCreateCount = 0;
    std::uint64_t m_NextResourceCreationSerial = 1;
    ResourceRegistry<TextureHandle, TextureDesc> m_Textures{1u};
    ResourceRegistry<BufferHandle, BufferDesc> m_Buffers{1001u};
    ResourceRegistry<ShaderHandle, ShaderDesc> m_Shaders{2001u};
    ResourceRegistry<PipelineHandle, PipelineDesc> m_Pipelines{3001u};
    ResourceRegistry<SamplerHandle, SamplerDesc> m_Samplers{4001u};
    ResourceRegistry<BindingLayoutHandle, GraphicsBindingLayoutDesc>
        m_BindingLayouts{5001u};
    ResourceRegistry<BindingSetHandle, GraphicsBindingSetDesc>
        m_BindingSets{6001u};
};

NullGraphicsBackend::NullGraphicsBackend()
    : m_Impl(std::make_unique<Impl>())
{
}

NullGraphicsBackend::~NullGraphicsBackend() = default;

NullGraphicsBackend::NullGraphicsBackend(NullGraphicsBackend&&) noexcept =
    default;

NullGraphicsBackend& NullGraphicsBackend::operator=(
    NullGraphicsBackend&&) noexcept = default;

bool NullGraphicsBackend::Initialize(
    const RenderBackendDesc& backend,
    const SwapchainDesc& swapchain)
{
    if (!m_Impl)
    {
        return false;
    }

    return m_Impl->Initialize(backend, swapchain);
}

void NullGraphicsBackend::Shutdown()
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->Shutdown();
}

void NullGraphicsBackend::Resize(std::uint32_t width, std::uint32_t height)
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->Resize(width, height);
}

TextureHandle NullGraphicsBackend::CreateTexture(const TextureDesc& desc)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreateTexture(desc);
}

TextureHandle NullGraphicsBackend::CreateTexture(
    const TextureDesc& desc,
    GraphicsDataView initialData)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreateTexture(desc, initialData);
}

BufferHandle NullGraphicsBackend::CreateBuffer(const BufferDesc& desc)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreateBuffer(desc);
}

BufferHandle NullGraphicsBackend::CreateBuffer(
    const BufferDesc& desc,
    GraphicsDataView initialData)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreateBuffer(desc, initialData);
}

ShaderHandle NullGraphicsBackend::CreateShader(const ShaderDesc& desc)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreateShader(desc);
}

PipelineHandle NullGraphicsBackend::CreatePipeline(const PipelineDesc& desc)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreatePipeline(desc);
}

SamplerHandle NullGraphicsBackend::CreateSampler(const SamplerDesc& desc)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreateSampler(desc);
}

BindingLayoutHandle NullGraphicsBackend::CreateBindingLayout(
    const GraphicsBindingLayoutDesc& desc)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreateBindingLayout(desc);
}

BindingSetHandle NullGraphicsBackend::CreateBindingSet(
    const GraphicsBindingSetDesc& desc)
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->CreateBindingSet(desc);
}

void NullGraphicsBackend::DestroyTexture(TextureHandle handle)
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->DestroyTexture(handle);
}

void NullGraphicsBackend::DestroyBuffer(BufferHandle handle)
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->DestroyBuffer(handle);
}

void NullGraphicsBackend::DestroyShader(ShaderHandle handle)
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->DestroyShader(handle);
}

void NullGraphicsBackend::DestroyPipeline(PipelineHandle handle)
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->DestroyPipeline(handle);
}

void NullGraphicsBackend::DestroySampler(SamplerHandle handle)
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->DestroySampler(handle);
}

void NullGraphicsBackend::DestroyBindingLayout(BindingLayoutHandle handle)
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->DestroyBindingLayout(handle);
}

void NullGraphicsBackend::DestroyBindingSet(BindingSetHandle handle)
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->DestroyBindingSet(handle);
}

void NullGraphicsBackend::BeginFrame()
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->BeginFrame();
}

void NullGraphicsBackend::EndFrame()
{
    if (!m_Impl)
    {
        return;
    }

    m_Impl->EndFrame();
}

RenderBackendStatus NullGraphicsBackend::GetStatus() const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->GetStatus();
}

RenderBackendCapabilities NullGraphicsBackend::GetCapabilities()
    const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->GetCapabilities();
}

GraphicsResourceMemoryReport NullGraphicsBackend::GetResourceMemoryReport()
    const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->GetResourceMemoryReport();
}

GraphicsResourceFailure NullGraphicsBackend::GetLastResourceFailure()
    const noexcept
{
    if (!m_Impl)
    {
        return GraphicsResourceFailure::BackendNotInitialized;
    }

    return m_Impl->GetLastResourceFailure();
}

GraphicsResourceLeakSummary
NullGraphicsBackend::GetLastShutdownResourceLeakSummary() const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->GetLastShutdownResourceLeakSummary();
}

GraphicsResourceQueryResult NullGraphicsBackend::QueryResource(
    GraphicsResourceKind kind,
    GraphicsHandle handle) const
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->QueryResource(kind, handle);
}

GraphicsBindingLayoutQueryResult NullGraphicsBackend::QueryBindingLayout(
    BindingLayoutHandle handle) const
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->QueryBindingLayout(handle);
}

GraphicsBindingSetQueryResult NullGraphicsBackend::QueryBindingSet(
    BindingSetHandle handle) const
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->QueryBindingSet(handle);
}

GraphicsResourceBacking NullGraphicsBackend::GetTextureBackingForTesting(
    TextureHandle handle) const noexcept
{
    if (!m_Impl)
    {
        return GraphicsResourceBacking::Invalid;
    }

    return m_Impl->GetTextureBackingForTesting(handle);
}

GraphicsResourceBacking NullGraphicsBackend::GetBufferBackingForTesting(
    BufferHandle handle) const noexcept
{
    if (!m_Impl)
    {
        return GraphicsResourceBacking::Invalid;
    }

    return m_Impl->GetBufferBackingForTesting(handle);
}

GraphicsResourceQueryResult NullGraphicsBackend::QueryTextureForTesting(
    TextureHandle handle) const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->QueryTextureForTesting(handle);
}

GraphicsResourceQueryResult NullGraphicsBackend::QueryBufferForTesting(
    BufferHandle handle) const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->QueryBufferForTesting(handle);
}

GraphicsResourceQueryResult NullGraphicsBackend::QueryShaderForTesting(
    ShaderHandle handle) const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->QueryShaderForTesting(handle);
}

GraphicsResourceQueryResult NullGraphicsBackend::QueryPipelineForTesting(
    PipelineHandle handle) const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->QueryPipelineForTesting(handle);
}

GraphicsResourceQueryResult NullGraphicsBackend::QuerySamplerForTesting(
    SamplerHandle handle) const noexcept
{
    if (!m_Impl)
    {
        return {};
    }

    return m_Impl->QuerySamplerForTesting(handle);
}

std::uint32_t NullGraphicsBackend::Width() const noexcept
{
    if (!m_Impl)
    {
        return 0u;
    }

    return m_Impl->Width();
}

std::uint32_t NullGraphicsBackend::Height() const noexcept
{
    if (!m_Impl)
    {
        return 0u;
    }

    return m_Impl->Height();
}

} // namespace SagaEngine::Graphics
