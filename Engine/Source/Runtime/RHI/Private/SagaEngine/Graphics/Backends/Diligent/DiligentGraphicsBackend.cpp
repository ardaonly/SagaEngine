/// @file DiligentGraphicsBackend.cpp
/// @brief Private SagaGraphics lifecycle adapter over the render backend.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"

#include "SagaEngine/Graphics/Backend/GraphicsRenderBackendMapping.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

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

} // anonymous namespace

DiligentGraphicsBackend::DiligentGraphicsBackend()
    : m_OwnedRuntime(std::make_unique<Runtime::DiligentGraphicsRuntime>())
    , m_Runtime(m_OwnedRuntime.get())
    , m_NativeOwner(&m_Runtime->NativeResources())
{
    m_LastCapabilities = MakeConservativeCapabilities();
}

DiligentGraphicsBackend::DiligentGraphicsBackend(
    Runtime::DiligentGraphicsRuntime& externalRuntime)
    : m_Runtime(&externalRuntime)
    , m_ExternalRuntime(true)
    , m_NativeOwner(&m_Runtime->NativeResources())
{
    m_LastCapabilities = MakeConservativeCapabilities();
}

DiligentGraphicsBackend::DiligentGraphicsBackend(
    bool forceHeadlessForTesting)
    : DiligentGraphicsBackend()
{
    m_ForceHeadlessForTesting = forceHeadlessForTesting;
}

DiligentGraphicsBackend::DiligentGraphicsBackend(
    bool forceHeadlessForTesting,
    bool forceReadyForTesting)
    : DiligentGraphicsBackend(forceHeadlessForTesting)
{
    m_ForceReadyForTesting = forceReadyForTesting;
}

DiligentGraphicsBackend::~DiligentGraphicsBackend() = default;

bool DiligentGraphicsBackend::Initialize(
    const RenderBackendDesc& backend,
    const SwapchainDesc& swapchain)
{
    if (m_ExternalRuntime)
    {
        ReleaseResources();
    }
    else
    {
        Shutdown();
    }
    m_NativeCreationEnabled = false;
    m_NativeOwner = &m_Runtime->NativeResources();
    m_LastStatus = {};
    m_LastStatus.selectedBackend = backend.preferredBackend;

    if (m_ExternalRuntime)
    {
        if (!m_Runtime->IsInitialized())
        {
            SetFailure(RenderBackendFailure::InitializationFailed);
            m_LastCapabilities = MakeConservativeCapabilities();
            return false;
        }

        m_Headless = false;
        m_LastStatus =
            Mapping::ToGraphicsBackendStatus(m_Runtime->Status());
        m_NativeCreationEnabled =
            m_NativeOwner && m_NativeOwner->CanCreateNative();
        m_LastCapabilities =
            MakeReadyCapabilities(m_LastStatus.selectedBackend);
        return m_NativeCreationEnabled;
    }

    if (m_ForceReadyForTesting)
    {
        m_Headless = false;
        m_LastStatus.frameIndex = 0;
        m_LastStatus.initialized = true;
        m_LastStatus.health = RenderBackendHealth::Ready;
        m_LastStatus.failure = RenderBackendFailure::None;
        m_LastCapabilities = MakeReadyCapabilities(backend.preferredBackend);
        return true;
    }

    m_Headless = backend.headless || m_ForceHeadlessForTesting;
    if (m_Headless)
    {
        m_HeadlessStatus.selectedBackend = backend.preferredBackend;
        m_HeadlessStatus.frameIndex = 0;
        m_HeadlessStatus.initialized = true;
        m_HeadlessStatus.health = RenderBackendHealth::Headless;
        m_HeadlessStatus.failure = RenderBackendFailure::None;
        m_LastStatus = m_HeadlessStatus;
        m_LastCapabilities = MakeConservativeCapabilities();
        m_LastCapabilities.backend = backend.preferredBackend;
        return true;
    }

    if (swapchain.width == 0u || swapchain.height == 0u)
    {
        m_SurfaceMinimized = true;
        SetFrameSkipped(RenderBackendFailure::InvalidSurfaceSize);
        return false;
    }

    const bool initialized =
        m_Runtime->Initialize(
            Mapping::ToRenderBackendConfig(backend),
            ToRenderSwapchainDesc(swapchain));
    m_LastStatus =
        Mapping::ToGraphicsBackendStatus(m_Runtime->Status());
    if (!initialized)
    {
        SetFailure(RenderBackendFailure::InitializationFailed);
        m_Runtime->Shutdown();
        m_LastCapabilities = MakeConservativeCapabilities();
        m_NativeOwner = &m_Runtime->NativeResources();
    }
    else
    {
        m_NativeOwner = &m_Runtime->NativeResources();
        m_NativeCreationEnabled =
            m_NativeOwner && m_NativeOwner->CanCreateNative();
        m_LastCapabilities = MakeReadyCapabilities(m_LastStatus.selectedBackend);
    }

    return initialized;
}

void DiligentGraphicsBackend::Shutdown()
{
    m_LastShutdownLeakSummary = BuildLeakSummary(BuildResourceMemoryReport());
    ReleaseResources();

    if (m_Runtime && !m_ExternalRuntime)
    {
        m_Runtime->Shutdown();
    }

    m_NativeOwner = m_Runtime ? &m_Runtime->NativeResources() : nullptr;
    m_NativeCreationEnabled = false;
    m_HeadlessStatus.initialized = false;
    m_HeadlessStatus.health = RenderBackendHealth::Shutdown;
    m_LastStatus.initialized = false;
    m_LastStatus.health = RenderBackendHealth::Shutdown;
    m_LastCapabilities = MakeConservativeCapabilities();
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
        m_Runtime->Resize(width, height);
        m_LastStatus = GetStatus();
    }
}

std::uint64_t DiligentGraphicsBackend::PackHandleKey(
    std::uint32_t index,
    std::uint32_t generation) noexcept
{
    return (static_cast<std::uint64_t>(generation) << 32u) |
           static_cast<std::uint64_t>(index);
}


RenderBackendCapabilities
DiligentGraphicsBackend::MakeConservativeCapabilities() const noexcept
{
    RenderBackendCapabilities capabilities{};
    capabilities.backend = m_LastStatus.selectedBackend;
    capabilities.qualityCeiling = RenderQualityPreset::Low;
    capabilities.maxTexture2DSize = 1024u;
    capabilities.maxColorAttachments = 1u;
    capabilities.maxFramesInFlight = 1u;
    return capabilities;
}

RenderBackendCapabilities DiligentGraphicsBackend::MakeReadyCapabilities(
    BackendPreference backend) const noexcept
{
    RenderBackendCapabilities capabilities{};
    capabilities.backend = backend;
    capabilities.qualityCeiling = RenderQualityPreset::Medium;
    capabilities.maxTexture2DSize = 4096u;
    capabilities.maxColorAttachments = 1u;
    capabilities.maxFramesInFlight = 1u;
    return capabilities;
}

bool DiligentGraphicsBackend::CanRenderFrame() const noexcept
{
    return m_Runtime && !m_SurfaceMinimized &&
           m_LastStatus.initialized &&
           m_LastStatus.failure == RenderBackendFailure::None;
}

void DiligentGraphicsBackend::SetFailure(
    RenderBackendFailure failure) noexcept
{
    m_LastStatus.initialized = false;
    m_LastStatus.health = RenderBackendHealth::Failed;
    m_LastStatus.failure = failure;
    m_LastCapabilities = MakeConservativeCapabilities();
}

void DiligentGraphicsBackend::SetFrameSkipped(
    RenderBackendFailure failure) noexcept
{
    m_LastStatus.initialized = false;
    m_LastStatus.health = RenderBackendHealth::FrameSkipped;
    m_LastStatus.failure = failure;
    m_LastCapabilities = MakeConservativeCapabilities();
}

std::unique_ptr<IGraphicsBackend> CreateDiligentGraphicsBackend()
{
    return std::make_unique<DiligentGraphicsBackend>();
}

std::unique_ptr<IGraphicsBackend> CreateDiligentGraphicsBackendForTesting(
)
{
    return std::make_unique<DiligentGraphicsBackend>(true);
}

} // namespace SagaEngine::Graphics::Backends::Diligent
