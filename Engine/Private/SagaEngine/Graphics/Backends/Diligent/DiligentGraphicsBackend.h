/// @file DiligentGraphicsBackend.h
/// @brief Private SagaGraphics lifecycle adapter over the render backend.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace RenderBackend = ::SagaEngine::Render::Backend;

class DiligentGraphicsBackend final : public IGraphicsBackend
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
    [[nodiscard]] std::uint64_t GetTextureShadowBytesForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetBufferShadowBytesForTesting(
        BufferHandle handle) const noexcept;

private:
    [[nodiscard]] RenderBackendCapabilities MakeConservativeCapabilities()
        const noexcept;
    [[nodiscard]] RenderBackendCapabilities MakeReadyCapabilities(
        BackendPreference backend) const noexcept;
    [[nodiscard]] bool CanRenderFrame() const noexcept;
    [[nodiscard]] bool CanCreateResources() const noexcept;
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
    bool m_Headless = false;
    bool m_SurfaceMinimized = false;
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
