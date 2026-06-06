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

struct RenderBackendStatus
{
    BackendPreference selectedBackend = BackendPreference::Auto;
    std::uint64_t     frameIndex      = 0;
    bool              initialized     = false;
};

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
        return true;
    }

    void Shutdown() override
    {
        m_Status.initialized = false;
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
