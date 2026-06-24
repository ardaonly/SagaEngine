/// @file DiligentGraphicsBackend.h
/// @brief Private SagaGraphics lifecycle adapter over the render backend.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace SagaEngine::Render::Backend
{
class DiligentNativeResourceOwner;
}

namespace Diligent
{
struct IBuffer;
struct ITextureView;
struct ISampler;
struct IShader;
struct IPipelineState;
}

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace RenderBackend = ::SagaEngine::Render::Backend;

class DiligentGraphicsBackend final : public IGraphicsBackend
{
private:
    struct NativeResourceOwnership
    {
        std::uint64_t serial = 0;
        bool uploadDeferred = false;
    };

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
            GraphicsResourceBacking backing =
                GraphicsResourceBacking::RegisteredOnly,
            NativeResourceOwnership nativeOwnership = {},
            std::uint64_t creationSerial = 0,
            GraphicsResourceLifecycle lifecycle =
                GraphicsResourceLifecycle::RegisteredOnly)
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
                slot.backing = backing;
                slot.nativeOwnership = nativeOwnership;
                slot.lifecycle = lifecycle;
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
                        backing,
                        nativeOwnership,
                        lifecycle,
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

        [[nodiscard]] bool UpdateNativeState(
            HandleT handle,
            GraphicsResourceBacking backing,
            NativeResourceOwnership nativeOwnership,
            GraphicsResourceLifecycle lifecycle) noexcept
        {
            if (!handle.IsValid() || handle.index > m_Slots.size())
            {
                return false;
            }

            auto& slot = m_Slots[handle.index - 1u];
            if (!slot.occupied || slot.generation != handle.generation)
            {
                return false;
            }

            slot.backing = backing;
            slot.nativeOwnership = nativeOwnership;
            slot.lifecycle = lifecycle;
            return true;
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
            slot.nativeOwnership = {};
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
                slot.nativeOwnership = {};
                slot.lifecycle = GraphicsResourceLifecycle::Retired;
                slot.lastUseSerial = slot.creationSerial;
                m_FreeSlots.push_back(i);
            }
            m_LiveCount = 0;
            m_LiveBytes = 0;
        }

        [[nodiscard]] std::uint64_t ShadowBytes(HandleT handle)
            const noexcept
        {
            if (!handle.IsValid() || handle.index > m_Slots.size())
            {
                return 0u;
            }

            const auto slotIndex = handle.index - 1u;
            const auto& slot = m_Slots[slotIndex];
            if (!slot.occupied || slot.generation != handle.generation)
            {
                return 0u;
            }

            return static_cast<std::uint64_t>(slot.shadowPayload.size());
        }

        [[nodiscard]] std::uint64_t NativeSerial(HandleT handle)
            const noexcept
        {
            if (!handle.IsValid() || handle.index > m_Slots.size())
            {
                return 0u;
            }

            const auto slotIndex = handle.index - 1u;
            const auto& slot = m_Slots[slotIndex];
            if (!slot.occupied || slot.generation != handle.generation)
            {
                return 0u;
            }

            return slot.nativeOwnership.serial;
        }

        [[nodiscard]] bool NativeUploadDeferred(HandleT handle)
            const noexcept
        {
            if (!handle.IsValid() || handle.index > m_Slots.size())
            {
                return false;
            }

            const auto slotIndex = handle.index - 1u;
            const auto& slot = m_Slots[slotIndex];
            if (!slot.occupied || slot.generation != handle.generation)
            {
                return false;
            }

            return slot.nativeOwnership.uploadDeferred;
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

    private:
        struct Slot
        {
            DescT desc{};
            std::uint64_t estimatedBytes = 0;
            std::vector<std::uint8_t> shadowPayload{};
            GraphicsResourceBacking backing =
                GraphicsResourceBacking::RegisteredOnly;
            NativeResourceOwnership nativeOwnership{};
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
    using StatusReader = RenderBackend::RenderBackendStatus (*)(
        const RenderBackend::IRenderBackend&) noexcept;
    using BackendFactory = std::unique_ptr<RenderBackend::IRenderBackend> (*)(
        const RenderBackendDesc&);

    DiligentGraphicsBackend();
    explicit DiligentGraphicsBackend(
        std::unique_ptr<RenderBackend::IRenderBackend> backend,
        StatusReader statusReader = RenderBackend::GetRenderBackendStatus);
    explicit DiligentGraphicsBackend(
        BackendFactory backendFactory,
        StatusReader statusReader = RenderBackend::GetRenderBackendStatus);
    ~DiligentGraphicsBackend() override;

    [[nodiscard]] bool Initialize(
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

    void DestroyTexture(TextureHandle handle) override;
    void DestroyBuffer(BufferHandle handle) override;
    void DestroyShader(ShaderHandle handle) override;
    void DestroyPipeline(PipelineHandle handle) override;
    void DestroySampler(SamplerHandle handle) override;

    void BeginFrame() override;
    void EndFrame() override;

    [[nodiscard]] RenderBackendStatus GetStatus() const noexcept override;
    [[nodiscard]] RenderBackendCapabilities
    GetCapabilities() const noexcept override;
    [[nodiscard]] GraphicsResourceMemoryReport GetResourceMemoryReport()
        const noexcept override;
    [[nodiscard]] GraphicsResourceFailure GetLastResourceFailure()
        const noexcept override;
    [[nodiscard]] GraphicsResourceLeakSummary
    GetLastShutdownResourceLeakSummary() const noexcept override;
    [[nodiscard]] GraphicsResourceQueryResult QueryResource(
        GraphicsResourceKind kind,
        GraphicsHandle handle) const override;
    [[nodiscard]] std::uint64_t GetTextureShadowBytesForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetBufferShadowBytesForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetTextureNativeSerialForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetBufferNativeSerialForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetSamplerNativeSerialForTesting(
        SamplerHandle handle) const noexcept;
    [[nodiscard]] bool GetTextureNativeUploadDeferredForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] bool GetBufferNativeUploadDeferredForTesting(
        BufferHandle handle) const noexcept;
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
    [[nodiscard]] bool HasNativeDeviceServicesForTesting() const noexcept;
    void BindNativeDeviceServicesForTesting(
        RenderBackend::DiligentDeviceServices services,
        bool enableNativeCreation = false) noexcept;
    [[nodiscard]] bool MarkBufferNativeForTesting(
        BufferHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] bool MarkTextureNativeForTesting(
        TextureHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] bool MarkSamplerNativeForTesting(
        SamplerHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] bool MarkShaderNativeForTesting(
        ShaderHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] bool MarkPipelineNativeForTesting(
        PipelineHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] ::Diligent::IBuffer* ResolveNativeBufferForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] ::Diligent::ITextureView* ResolveNativeTextureSrvForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] ::Diligent::ISampler* ResolveNativeSamplerForTesting(
        SamplerHandle handle) const noexcept;
    [[nodiscard]] ::Diligent::IShader* ResolveNativeShaderForTesting(
        ShaderHandle handle) const noexcept;
    [[nodiscard]] ::Diligent::IPipelineState* ResolveNativePipelineForTesting(
        PipelineHandle handle) const noexcept;

private:
    [[nodiscard]] RenderBackendCapabilities MakeConservativeCapabilities()
        const noexcept;
    [[nodiscard]] RenderBackendCapabilities MakeReadyCapabilities(
        BackendPreference backend) const noexcept;
    [[nodiscard]] bool CanRenderFrame() const noexcept;
    [[nodiscard]] bool CanCreateResources() const noexcept;
    [[nodiscard]] GraphicsResourceBacking ResourceBackingForNativeResource()
        const noexcept;
    [[nodiscard]] GraphicsResourceLifecycle ResourceLifecycleForBacking(
        GraphicsResourceBacking backing) const noexcept;
    [[nodiscard]] NativeResourceOwnership MakeNativeResourceOwnership(
        bool uploadDeferred) noexcept;
    template <typename HandleT>
    [[nodiscard]] HandleT RecordCreateFailure(
        GraphicsResourceFailure failure) noexcept;
    template <typename HandleT>
    [[nodiscard]] HandleT RecordSuccessfulCreate(HandleT handle) noexcept;
    [[nodiscard]] GraphicsResourceMemoryReport BuildResourceMemoryReport()
        const noexcept;
    [[nodiscard]] static constexpr GraphicsResourceLeakSummary
    BuildLeakSummary(const GraphicsResourceMemoryReport& report) noexcept;
    void SetFailure(RenderBackendFailure failure) noexcept;
    void SetFrameSkipped(RenderBackendFailure failure) noexcept;
    void ReleaseResources() noexcept;

    std::unique_ptr<RenderBackend::IRenderBackend> m_RenderBackend;
    BackendFactory m_BackendFactory = nullptr;
    StatusReader m_StatusReader = RenderBackend::GetRenderBackendStatus;
    RenderBackendStatus m_HeadlessStatus{};
    RenderBackendStatus m_LastStatus{};
    RenderBackendCapabilities m_LastCapabilities{};
    RenderBackend::DiligentDeviceServices m_DeviceServices{};
    std::unique_ptr<RenderBackend::DiligentNativeResourceOwner> m_NativeOwner;
    bool m_NativeCreationEnabled = false;
    bool m_Headless = false;
    bool m_SurfaceMinimized = false;
    GraphicsResourceFailure m_LastResourceFailure =
        GraphicsResourceFailure::None;
    GraphicsResourceLeakSummary m_LastShutdownLeakSummary{};
    std::uint64_t m_PeakLiveBytes = 0;
    std::uint64_t m_FailedCreateCount = 0;
    std::uint64_t m_NextNativeResourceSerial = 1;
    std::uint64_t m_NextResourceCreationSerial = 1;
    ResourceRegistry<TextureHandle, TextureDesc> m_Textures{1u};
    ResourceRegistry<BufferHandle, BufferDesc> m_Buffers{1001u};
    ResourceRegistry<ShaderHandle, ShaderDesc> m_Shaders{2001u};
    ResourceRegistry<PipelineHandle, PipelineDesc> m_Pipelines{3001u};
    ResourceRegistry<SamplerHandle, SamplerDesc> m_Samplers{4001u};
};

[[nodiscard]] std::unique_ptr<IGraphicsBackend> CreateDiligentGraphicsBackend();
[[nodiscard]] std::unique_ptr<IGraphicsBackend>
CreateDiligentGraphicsBackendForTesting(
    std::unique_ptr<RenderBackend::IRenderBackend> backend,
    DiligentGraphicsBackend::StatusReader statusReader =
        RenderBackend::GetRenderBackendStatus);
[[nodiscard]] std::unique_ptr<IGraphicsBackend>
CreateDiligentGraphicsBackendForTesting(
    DiligentGraphicsBackend::BackendFactory backendFactory,
    DiligentGraphicsBackend::StatusReader statusReader =
        RenderBackend::GetRenderBackendStatus);

} // namespace SagaEngine::Graphics::Backends::Diligent
