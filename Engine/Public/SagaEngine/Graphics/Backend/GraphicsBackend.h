/// @file GraphicsBackend.h
/// @brief Minimal vendor-neutral graphics backend shell.

#pragma once

#include "SagaEngine/Graphics/Core/GraphicsTypes.h"
#include "SagaEngine/Graphics/Descs/ResourceDescs.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

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
private:
    template <typename HandleT, typename DescT>
    class ResourceRegistry
    {
    public:
        [[nodiscard]] HandleT Create(
            const DescT& desc,
            bool canCreate)
        {
            if (!canCreate)
            {
                return {};
            }

            std::uint32_t slotIndex = 0;
            if (!m_FreeSlots.empty())
            {
                slotIndex = m_FreeSlots.back();
                m_FreeSlots.pop_back();
                auto& slot = m_Slots[slotIndex];
                slot.desc = desc;
                slot.occupied = true;
                slot.generation = NextGeneration(slot.generation);
            }
            else
            {
                slotIndex = static_cast<std::uint32_t>(m_Slots.size());
                m_Slots.push_back({desc, 1u, true});
            }

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
            m_FreeSlots.push_back(slotIndex);
        }

        void ReleaseAll()
        {
            m_FreeSlots.clear();
            for (std::uint32_t i = 0; i < m_Slots.size(); ++i)
            {
                auto& slot = m_Slots[i];
                slot.occupied = false;
                m_FreeSlots.push_back(i);
            }
        }

    private:
        struct Slot
        {
            DescT desc{};
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
        return m_Textures.Create(desc, CanCreateResources());
    }

    BufferHandle CreateBuffer(const BufferDesc& desc) override
    {
        return m_Buffers.Create(desc, CanCreateResources());
    }

    ShaderHandle CreateShader(const ShaderDesc& desc) override
    {
        return m_Shaders.Create(desc, CanCreateResources());
    }

    PipelineHandle CreatePipeline(const PipelineDesc& desc) override
    {
        return m_Pipelines.Create(desc, CanCreateResources());
    }

    SamplerHandle CreateSampler(const SamplerDesc& desc) override
    {
        return m_Samplers.Create(desc, CanCreateResources());
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

    RenderBackendStatus m_Status{};
    std::uint32_t       m_Width = 0;
    std::uint32_t       m_Height = 0;
    ResourceRegistry<TextureHandle, TextureDesc> m_Textures;
    ResourceRegistry<BufferHandle, BufferDesc> m_Buffers;
    ResourceRegistry<ShaderHandle, ShaderDesc> m_Shaders;
    ResourceRegistry<PipelineHandle, PipelineDesc> m_Pipelines;
    ResourceRegistry<SamplerHandle, SamplerDesc> m_Samplers;
};

} // namespace SagaEngine::Graphics
