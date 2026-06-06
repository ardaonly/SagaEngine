/// @file DiligentGraphicsBackend.cpp
/// @brief Private SagaGraphics lifecycle adapter over the render backend.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"

#include "SagaEngine/Graphics/Backend/GraphicsRenderBackendMapping.h"

#include <utility>

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace Mapping = ::SagaEngine::Graphics::Backend;

namespace
{

[[nodiscard]] constexpr RenderBackend::SwapchainDesc ToRenderSwapchainDesc(
    const SwapchainDesc& desc) noexcept
{
    RenderBackend::SwapchainDesc renderDesc{};
    renderDesc.nativeWindow = desc.nativeWindow;
    renderDesc.width = desc.width;
    renderDesc.height = desc.height;
    renderDesc.vsync = desc.vsync;
    renderDesc.hdr = desc.highDynamicRange;
    return renderDesc;
}

} // namespace

DiligentGraphicsBackend::DiligentGraphicsBackend() = default;

DiligentGraphicsBackend::DiligentGraphicsBackend(
    std::unique_ptr<RenderBackend::IRenderBackend> backend,
    StatusReader statusReader)
    : m_RenderBackend(std::move(backend))
    , m_StatusReader(statusReader ? statusReader
                                  : RenderBackend::GetRenderBackendStatus)
{
}

bool DiligentGraphicsBackend::Initialize(
    const RenderBackendDesc& backend,
    const SwapchainDesc& swapchain)
{
    Shutdown();

    m_Headless = backend.headless;
    if (m_Headless)
    {
        m_HeadlessStatus.selectedBackend = backend.preferredBackend;
        m_HeadlessStatus.frameIndex = 0;
        m_HeadlessStatus.initialized = true;
        m_LastStatus = m_HeadlessStatus;
        return true;
    }

    if (!m_RenderBackend)
    {
        m_RenderBackend =
            RenderBackend::CreateRenderBackend(Mapping::ToRenderBackendConfig(backend));
    }

    if (!m_RenderBackend)
    {
        m_LastStatus = {};
        return false;
    }

    const bool initialized =
        m_RenderBackend->Initialize(ToRenderSwapchainDesc(swapchain));
    m_LastStatus = GetStatus();
    return initialized;
}

void DiligentGraphicsBackend::Shutdown()
{
    if (m_RenderBackend)
    {
        m_RenderBackend->Shutdown();
    }

    m_HeadlessStatus.initialized = false;
    m_LastStatus.initialized = false;
    m_Headless = false;
}

void DiligentGraphicsBackend::Resize(std::uint32_t width, std::uint32_t height)
{
    if (m_RenderBackend)
    {
        m_RenderBackend->OnResize(width, height);
    }
}

TextureHandle DiligentGraphicsBackend::CreateTexture(const TextureDesc&)
{
    return {};
}

BufferHandle DiligentGraphicsBackend::CreateBuffer(const BufferDesc&)
{
    return {};
}

ShaderHandle DiligentGraphicsBackend::CreateShader(const ShaderDesc&)
{
    return {};
}

PipelineHandle DiligentGraphicsBackend::CreatePipeline(const PipelineDesc&)
{
    return {};
}

SamplerHandle DiligentGraphicsBackend::CreateSampler(const SamplerDesc&)
{
    return {};
}

void DiligentGraphicsBackend::DestroyTexture(TextureHandle) {}
void DiligentGraphicsBackend::DestroyBuffer(BufferHandle) {}
void DiligentGraphicsBackend::DestroyShader(ShaderHandle) {}
void DiligentGraphicsBackend::DestroyPipeline(PipelineHandle) {}
void DiligentGraphicsBackend::DestroySampler(SamplerHandle) {}

void DiligentGraphicsBackend::BeginFrame()
{
    if (m_Headless && m_HeadlessStatus.initialized)
    {
        ++m_HeadlessStatus.frameIndex;
        m_LastStatus = m_HeadlessStatus;
        return;
    }

    if (m_RenderBackend)
    {
        m_RenderBackend->BeginFrame();
        m_LastStatus = GetStatus();
    }
}

void DiligentGraphicsBackend::EndFrame()
{
    if (m_RenderBackend)
    {
        m_RenderBackend->EndFrame();
        m_LastStatus = GetStatus();
    }
}

RenderBackendStatus DiligentGraphicsBackend::GetStatus() const noexcept
{
    if (m_Headless)
    {
        return m_HeadlessStatus;
    }

    if (!m_RenderBackend)
    {
        return m_LastStatus;
    }

    return Mapping::ToGraphicsBackendStatus(m_StatusReader(*m_RenderBackend));
}

std::unique_ptr<IGraphicsBackend> CreateDiligentGraphicsBackend()
{
    return std::make_unique<DiligentGraphicsBackend>();
}

std::unique_ptr<IGraphicsBackend> CreateDiligentGraphicsBackendForTesting(
    std::unique_ptr<RenderBackend::IRenderBackend> backend,
    DiligentGraphicsBackend::StatusReader statusReader)
{
    return std::make_unique<DiligentGraphicsBackend>(
        std::move(backend),
        statusReader);
}

} // namespace SagaEngine::Graphics::Backends::Diligent
