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

[[nodiscard]] std::unique_ptr<RenderBackend::IRenderBackend>
CreateDefaultRenderBackend(const RenderBackendDesc& backend)
{
    return RenderBackend::CreateRenderBackend(
        Mapping::ToRenderBackendConfig(backend));
}

} // namespace

DiligentGraphicsBackend::DiligentGraphicsBackend()
    : m_BackendFactory(CreateDefaultRenderBackend)
{
}

DiligentGraphicsBackend::DiligentGraphicsBackend(
    std::unique_ptr<RenderBackend::IRenderBackend> backend,
    StatusReader statusReader)
    : m_RenderBackend(std::move(backend))
    , m_BackendFactory(CreateDefaultRenderBackend)
    , m_StatusReader(statusReader ? statusReader
                                  : RenderBackend::GetRenderBackendStatus)
{
}

DiligentGraphicsBackend::DiligentGraphicsBackend(
    BackendFactory backendFactory,
    StatusReader statusReader)
    : m_BackendFactory(backendFactory ? backendFactory
                                      : CreateDefaultRenderBackend)
    , m_StatusReader(statusReader ? statusReader
                                  : RenderBackend::GetRenderBackendStatus)
{
}

bool DiligentGraphicsBackend::Initialize(
    const RenderBackendDesc& backend,
    const SwapchainDesc& swapchain)
{
    Shutdown();
    m_LastStatus = {};
    m_LastStatus.selectedBackend = backend.preferredBackend;

    m_Headless = backend.headless;
    if (m_Headless)
    {
        m_HeadlessStatus.selectedBackend = backend.preferredBackend;
        m_HeadlessStatus.frameIndex = 0;
        m_HeadlessStatus.initialized = true;
        m_HeadlessStatus.health = RenderBackendHealth::Headless;
        m_HeadlessStatus.failure = RenderBackendFailure::None;
        m_LastStatus = m_HeadlessStatus;
        return true;
    }

    if (swapchain.width == 0u || swapchain.height == 0u)
    {
        m_SurfaceMinimized = true;
        SetFrameSkipped(RenderBackendFailure::InvalidSurfaceSize);
        return false;
    }

    if (!m_RenderBackend)
    {
        m_RenderBackend = m_BackendFactory(backend);
    }

    if (!m_RenderBackend)
    {
        SetFailure(RenderBackendFailure::BackendUnavailable);
        return false;
    }

    const bool initialized =
        m_RenderBackend->Initialize(ToRenderSwapchainDesc(swapchain));
    m_LastStatus =
        Mapping::ToGraphicsBackendStatus(m_StatusReader(*m_RenderBackend));
    if (!initialized)
    {
        SetFailure(RenderBackendFailure::InitializationFailed);
        m_RenderBackend->Shutdown();
    }

    return initialized;
}

void DiligentGraphicsBackend::Shutdown()
{
    if (m_RenderBackend)
    {
        m_RenderBackend->Shutdown();
    }

    m_HeadlessStatus.initialized = false;
    m_HeadlessStatus.health = RenderBackendHealth::Shutdown;
    m_LastStatus.initialized = false;
    m_LastStatus.health = RenderBackendHealth::Shutdown;
    m_Headless = false;
    m_SurfaceMinimized = false;
}

void DiligentGraphicsBackend::Resize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0u || height == 0u)
    {
        m_SurfaceMinimized = true;
        SetFrameSkipped(RenderBackendFailure::InvalidSurfaceSize);
        return;
    }

    m_SurfaceMinimized = false;
    if (CanRenderFrame())
    {
        m_RenderBackend->OnResize(width, height);
        m_LastStatus = GetStatus();
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

    if (!CanRenderFrame())
    {
        SetFrameSkipped(m_LastStatus.failure);
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
    if (!CanRenderFrame())
    {
        SetFrameSkipped(m_LastStatus.failure);
        return;
    }

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

    if (!m_RenderBackend || !m_LastStatus.initialized || m_SurfaceMinimized)
    {
        return m_LastStatus;
    }

    return Mapping::ToGraphicsBackendStatus(m_StatusReader(*m_RenderBackend));
}

bool DiligentGraphicsBackend::CanRenderFrame() const noexcept
{
    return m_RenderBackend && !m_SurfaceMinimized &&
           m_LastStatus.initialized &&
           m_LastStatus.failure == RenderBackendFailure::None;
}

void DiligentGraphicsBackend::SetFailure(
    RenderBackendFailure failure) noexcept
{
    m_LastStatus.initialized = false;
    m_LastStatus.health = RenderBackendHealth::Failed;
    m_LastStatus.failure = failure;
}

void DiligentGraphicsBackend::SetFrameSkipped(
    RenderBackendFailure failure) noexcept
{
    m_LastStatus.initialized = false;
    m_LastStatus.health = RenderBackendHealth::FrameSkipped;
    m_LastStatus.failure = failure;
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

std::unique_ptr<IGraphicsBackend> CreateDiligentGraphicsBackendForTesting(
    DiligentGraphicsBackend::BackendFactory backendFactory,
    DiligentGraphicsBackend::StatusReader statusReader)
{
    return std::make_unique<DiligentGraphicsBackend>(
        backendFactory,
        statusReader);
}

} // namespace SagaEngine::Graphics::Backends::Diligent
