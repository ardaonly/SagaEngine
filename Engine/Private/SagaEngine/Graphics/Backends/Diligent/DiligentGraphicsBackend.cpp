/// @file DiligentGraphicsBackend.cpp
/// @brief Private SagaGraphics lifecycle adapter over the render backend.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"

#include "SagaEngine/Graphics/Backend/GraphicsRenderBackendMapping.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"

#include <cstddef>
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

[[nodiscard]] constexpr bool IsKnownFormat(ResourceFormat format) noexcept
{
    return format == ResourceFormat::Rgba8Unorm ||
           format == ResourceFormat::Rgba8UnormSrgb ||
           format == ResourceFormat::Bgra8Unorm ||
           format == ResourceFormat::Rgba16Float ||
           format == ResourceFormat::Rgba32Float ||
           format == ResourceFormat::Depth24Stencil8 ||
           format == ResourceFormat::Depth32Float;
}

[[nodiscard]] constexpr bool IsValidTextureDesc(
    const TextureDesc& desc) noexcept
{
    return desc.width != 0u && desc.height != 0u && desc.depth != 0u &&
           desc.mipLevels != 0u && desc.arrayLayers != 0u &&
           IsKnownFormat(desc.format);
}

[[nodiscard]] constexpr bool IsValidBufferDesc(const BufferDesc& desc) noexcept
{
    return desc.sizeBytes != 0u &&
           RenderBackend::DiligentNativeResourceOwner::IsBufferUsageSupported(
               desc.usage);
}

[[nodiscard]] constexpr bool IsValidShaderStage(ShaderStage stage) noexcept
{
    return stage == ShaderStage::Vertex || stage == ShaderStage::Fragment ||
           stage == ShaderStage::Compute;
}

[[nodiscard]] constexpr bool IsValidShaderDesc(const ShaderDesc& desc) noexcept
{
    return (desc.byteSize != 0u || !desc.source.empty()) &&
           IsValidShaderStage(desc.stage) &&
           (desc.source.empty() || !desc.entryPoint.empty());
}

[[nodiscard]] constexpr bool IsValidTopology(
    PrimitiveTopology topology) noexcept
{
    return topology == PrimitiveTopology::TriangleList ||
           topology == PrimitiveTopology::TriangleStrip ||
           topology == PrimitiveTopology::LineList;
}

[[nodiscard]] constexpr bool IsValidPipelineDesc(
    const PipelineDesc& desc) noexcept
{
    return desc.colorTargetCount != 0u && desc.colorTargetCount <= 8u &&
           IsValidTopology(desc.topology) && IsKnownFormat(desc.colorFormat) &&
           IsKnownFormat(desc.depthFormat);
}

[[nodiscard]] constexpr bool IsValidFilterMode(FilterMode mode) noexcept
{
    return mode == FilterMode::Nearest || mode == FilterMode::Linear;
}

[[nodiscard]] constexpr bool IsValidAddressMode(AddressMode mode) noexcept
{
    return mode == AddressMode::ClampToEdge || mode == AddressMode::Repeat ||
           mode == AddressMode::MirrorRepeat;
}

[[nodiscard]] constexpr bool IsValidSamplerDesc(
    const SamplerDesc& desc) noexcept
{
    return IsValidFilterMode(desc.minFilter) &&
           IsValidFilterMode(desc.magFilter) &&
           IsValidAddressMode(desc.addressU) &&
           IsValidAddressMode(desc.addressV) &&
           IsValidAddressMode(desc.addressW);
}

[[nodiscard]] constexpr std::uint64_t BytesPerPixel(
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

[[nodiscard]] constexpr std::uint64_t EstimateTextureBytes(
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

[[nodiscard]] constexpr bool IsValidTextureInitialData(
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

[[nodiscard]] constexpr std::uint64_t EstimateBufferBytes(
    const BufferDesc& desc) noexcept
{
    return desc.sizeBytes;
}

[[nodiscard]] constexpr bool IsValidBufferInitialData(
    const BufferDesc& desc,
    GraphicsDataView initialData) noexcept
{
    if (initialData.sizeBytes == 0u)
    {
        return true;
    }

    return initialData.data && initialData.sizeBytes <= desc.sizeBytes;
}

[[nodiscard]] std::vector<std::uint8_t> CopyShadowPayload(
    GraphicsDataView initialData)
{
    std::vector<std::uint8_t> payload;
    if (!initialData.data || initialData.sizeBytes == 0u)
    {
        return payload;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(initialData.data);
    payload.assign(
        bytes,
        bytes + static_cast<std::size_t>(initialData.sizeBytes));
    return payload;
}

template <typename HandleT>
[[nodiscard]] constexpr HandleT ToTypedHandle(GraphicsHandle handle) noexcept
{
    HandleT typed{};
    typed.index = handle.index;
    typed.generation = handle.generation;
    return typed;
}

[[nodiscard]] constexpr std::uint64_t EstimateShaderBytes(
    const ShaderDesc& desc) noexcept
{
    return desc.source.empty()
               ? desc.byteSize
               : static_cast<std::uint64_t>(desc.source.size());
}

[[nodiscard]] constexpr std::uint64_t EstimatePipelineBytes(
    const PipelineDesc&) noexcept
{
    return 0u;
}

[[nodiscard]] constexpr std::uint64_t EstimateSamplerBytes(
    const SamplerDesc&) noexcept
{
    return 0u;
}

} // namespace

DiligentGraphicsBackend::DiligentGraphicsBackend()
    : m_BackendFactory(CreateDefaultRenderBackend)
    , m_NativeOwner(std::make_unique<RenderBackend::DiligentNativeResourceOwner>())
{
    m_LastCapabilities = MakeConservativeCapabilities();
}

DiligentGraphicsBackend::DiligentGraphicsBackend(
    std::unique_ptr<RenderBackend::IRenderBackend> backend,
    StatusReader statusReader)
    : m_RenderBackend(std::move(backend))
    , m_BackendFactory(CreateDefaultRenderBackend)
    , m_StatusReader(statusReader ? statusReader
                                  : RenderBackend::GetRenderBackendStatus)
    , m_NativeOwner(std::make_unique<RenderBackend::DiligentNativeResourceOwner>())
{
    m_LastCapabilities = MakeConservativeCapabilities();
}

DiligentGraphicsBackend::DiligentGraphicsBackend(
    BackendFactory backendFactory,
    StatusReader statusReader)
    : m_BackendFactory(backendFactory ? backendFactory
                                      : CreateDefaultRenderBackend)
    , m_StatusReader(statusReader ? statusReader
                                  : RenderBackend::GetRenderBackendStatus)
    , m_NativeOwner(std::make_unique<RenderBackend::DiligentNativeResourceOwner>())
{
    m_LastCapabilities = MakeConservativeCapabilities();
}

DiligentGraphicsBackend::~DiligentGraphicsBackend() = default;

bool DiligentGraphicsBackend::Initialize(
    const RenderBackendDesc& backend,
    const SwapchainDesc& swapchain)
{
    Shutdown();
    m_DeviceServices = {};
    m_NativeCreationEnabled = false;
    if (m_NativeOwner)
    {
        m_NativeOwner->Bind({});
    }
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
        m_LastCapabilities = MakeConservativeCapabilities();
        m_DeviceServices = {};
    }
    else
    {
        if (auto* diligentBackend =
                dynamic_cast<RenderBackend::DiligentRenderBackend*>(
                    m_RenderBackend.get()))
        {
            m_DeviceServices = diligentBackend->GetDiligentDeviceServices();
            m_NativeCreationEnabled = m_DeviceServices.IsBound();
            if (m_NativeOwner)
            {
                m_NativeOwner->Bind(m_DeviceServices);
            }
        }
        else
        {
            m_DeviceServices = {};
            m_NativeCreationEnabled = false;
            if (m_NativeOwner)
            {
                m_NativeOwner->Bind({});
            }
        }
        m_LastCapabilities = MakeReadyCapabilities(m_LastStatus.selectedBackend);
    }

    return initialized;
}

void DiligentGraphicsBackend::Shutdown()
{
    m_LastShutdownLeakSummary = BuildLeakSummary(BuildResourceMemoryReport());
    ReleaseResources();

    if (m_RenderBackend)
    {
        m_RenderBackend->Shutdown();
    }

    m_DeviceServices = {};
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
        m_RenderBackend->OnResize(width, height);
        m_LastStatus = GetStatus();
    }
}

TextureHandle DiligentGraphicsBackend::CreateTexture(const TextureDesc& desc)
{
    return CreateTexture(desc, {});
}

TextureHandle DiligentGraphicsBackend::CreateTexture(
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

    const bool createNative =
        m_NativeCreationEnabled && m_NativeOwner &&
        m_NativeOwner->CanCreateNative();
    const auto backing =
        createNative ? GraphicsResourceBacking::RegisteredOnly
                     : ResourceBackingForNativeResource();
    auto handle = m_Textures.Create(
        desc,
        EstimateTextureBytes(desc),
        CopyShadowPayload(initialData),
        backing,
        createNative ? NativeResourceOwnership{}
                     : MakeNativeResourceOwnership(initialData.sizeBytes != 0u),
        m_NextResourceCreationSerial++,
        ResourceLifecycleForBacking(backing));

    if (createNative)
    {
        const auto serial =
            m_NativeOwner->CreateTextureForHandle(handle, desc, initialData);
        if (serial == 0u)
        {
            m_Textures.Destroy(handle);
            return RecordCreateFailure<TextureHandle>(
                GraphicsResourceFailure::InvalidTextureDesc);
        }
        m_Textures.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
            GraphicsResourceLifecycle::Ready);
    }

    return RecordSuccessfulCreate(handle);
}

BufferHandle DiligentGraphicsBackend::CreateBuffer(const BufferDesc& desc)
{
    return CreateBuffer(desc, {});
}

BufferHandle DiligentGraphicsBackend::CreateBuffer(
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

    const bool createNative =
        m_NativeCreationEnabled && m_NativeOwner &&
        m_NativeOwner->CanCreateNative();
    const auto backing =
        createNative ? GraphicsResourceBacking::RegisteredOnly
                     : ResourceBackingForNativeResource();
    auto handle = m_Buffers.Create(
        desc,
        EstimateBufferBytes(desc),
        CopyShadowPayload(initialData),
        backing,
        createNative ? NativeResourceOwnership{}
                     : MakeNativeResourceOwnership(initialData.sizeBytes != 0u),
        m_NextResourceCreationSerial++,
        ResourceLifecycleForBacking(backing));

    if (createNative)
    {
        const auto serial =
            m_NativeOwner->CreateBufferForHandle(handle, desc, initialData);
        if (serial == 0u)
        {
            m_Buffers.Destroy(handle);
            return RecordCreateFailure<BufferHandle>(
                GraphicsResourceFailure::InvalidBufferDesc);
        }
        m_Buffers.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
            GraphicsResourceLifecycle::Ready);
    }

    return RecordSuccessfulCreate(handle);
}

ShaderHandle DiligentGraphicsBackend::CreateShader(const ShaderDesc& desc)
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

    const bool createNative =
        m_NativeCreationEnabled && m_NativeOwner &&
        m_NativeOwner->CanCreateNative();
    auto handle = m_Shaders.Create(
        desc,
        EstimateShaderBytes(desc),
        {},
        GraphicsResourceBacking::RegisteredOnly,
        {},
        m_NextResourceCreationSerial++,
        GraphicsResourceLifecycle::RegisteredOnly);

    if (createNative)
    {
        const auto serial = m_NativeOwner->CreateShaderForHandle(handle, desc);
        if (serial == 0u)
        {
            m_Shaders.Destroy(handle);
            return RecordCreateFailure<ShaderHandle>(
                GraphicsResourceFailure::InvalidShaderDesc);
        }
        m_Shaders.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
            GraphicsResourceLifecycle::Ready);
    }

    return RecordSuccessfulCreate(handle);
}

PipelineHandle DiligentGraphicsBackend::CreatePipeline(
    const PipelineDesc& desc)
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

    const bool createNative =
        m_NativeCreationEnabled && m_NativeOwner &&
        m_NativeOwner->CanCreateNative();
    auto handle = m_Pipelines.Create(
        desc,
        EstimatePipelineBytes(desc),
        {},
        GraphicsResourceBacking::RegisteredOnly,
        {},
        m_NextResourceCreationSerial++,
        GraphicsResourceLifecycle::RegisteredOnly);

    if (createNative)
    {
        const auto serial =
            m_NativeOwner->CreatePipelineForHandle(handle, desc);
        if (serial == 0u)
        {
            m_Pipelines.Destroy(handle);
            return RecordCreateFailure<PipelineHandle>(
                GraphicsResourceFailure::InvalidPipelineDesc);
        }
        m_Pipelines.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
            GraphicsResourceLifecycle::Ready);
    }

    return RecordSuccessfulCreate(handle);
}

SamplerHandle DiligentGraphicsBackend::CreateSampler(const SamplerDesc& desc)
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

    const bool createNative =
        m_NativeCreationEnabled && m_NativeOwner &&
        m_NativeOwner->CanCreateNative();
    const auto backing =
        createNative ? GraphicsResourceBacking::RegisteredOnly
                     : ResourceBackingForNativeResource();
    auto handle = m_Samplers.Create(
        desc,
        EstimateSamplerBytes(desc),
        {},
        backing,
        createNative ? NativeResourceOwnership{} : MakeNativeResourceOwnership(false),
        m_NextResourceCreationSerial++,
        ResourceLifecycleForBacking(backing));

    if (createNative)
    {
        const auto serial = m_NativeOwner->CreateSamplerForHandle(handle, desc);
        if (serial == 0u)
        {
            m_Samplers.Destroy(handle);
            return RecordCreateFailure<SamplerHandle>(
                GraphicsResourceFailure::InvalidSamplerDesc);
        }
        m_Samplers.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
            GraphicsResourceLifecycle::Ready);
    }

    return RecordSuccessfulCreate(handle);
}

void DiligentGraphicsBackend::DestroyTexture(TextureHandle handle)
{
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyTexture(handle);
    }
    m_Textures.Destroy(handle);
}

void DiligentGraphicsBackend::DestroyBuffer(BufferHandle handle)
{
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyBuffer(handle);
    }
    m_Buffers.Destroy(handle);
}

void DiligentGraphicsBackend::DestroyShader(ShaderHandle handle)
{
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyShader(handle);
    }
    m_Shaders.Destroy(handle);
}

void DiligentGraphicsBackend::DestroyPipeline(PipelineHandle handle)
{
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyPipeline(handle);
    }
    m_Pipelines.Destroy(handle);
}

void DiligentGraphicsBackend::DestroySampler(SamplerHandle handle)
{
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroySampler(handle);
    }
    m_Samplers.Destroy(handle);
}

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

RenderBackendCapabilities DiligentGraphicsBackend::GetCapabilities()
    const noexcept
{
    return m_LastCapabilities;
}

GraphicsResourceMemoryReport DiligentGraphicsBackend::GetResourceMemoryReport()
    const noexcept
{
    return BuildResourceMemoryReport();
}

GraphicsResourceFailure DiligentGraphicsBackend::GetLastResourceFailure()
    const noexcept
{
    return m_LastResourceFailure;
}

GraphicsResourceLeakSummary
DiligentGraphicsBackend::GetLastShutdownResourceLeakSummary() const noexcept
{
    return m_LastShutdownLeakSummary;
}

GraphicsResourceQueryResult DiligentGraphicsBackend::QueryResource(
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
        return QueryPipelineForTesting(ToTypedHandle<PipelineHandle>(handle));
    case GraphicsResourceKind::Sampler:
        return QuerySamplerForTesting(ToTypedHandle<SamplerHandle>(handle));
    case GraphicsResourceKind::Invalid:
    default:
        return {};
    }
}

std::uint64_t DiligentGraphicsBackend::GetTextureShadowBytesForTesting(
    TextureHandle handle) const noexcept
{
    return m_Textures.ShadowBytes(handle);
}

std::uint64_t DiligentGraphicsBackend::GetBufferShadowBytesForTesting(
    BufferHandle handle) const noexcept
{
    return m_Buffers.ShadowBytes(handle);
}

std::uint64_t DiligentGraphicsBackend::GetTextureNativeSerialForTesting(
    TextureHandle handle) const noexcept
{
    return m_Textures.NativeSerial(handle);
}

std::uint64_t DiligentGraphicsBackend::GetBufferNativeSerialForTesting(
    BufferHandle handle) const noexcept
{
    return m_Buffers.NativeSerial(handle);
}

std::uint64_t DiligentGraphicsBackend::GetSamplerNativeSerialForTesting(
    SamplerHandle handle) const noexcept
{
    return m_Samplers.NativeSerial(handle);
}

bool DiligentGraphicsBackend::GetTextureNativeUploadDeferredForTesting(
    TextureHandle handle) const noexcept
{
    return m_Textures.NativeUploadDeferred(handle);
}

bool DiligentGraphicsBackend::GetBufferNativeUploadDeferredForTesting(
    BufferHandle handle) const noexcept
{
    return m_Buffers.NativeUploadDeferred(handle);
}

GraphicsResourceBacking DiligentGraphicsBackend::GetTextureBackingForTesting(
    TextureHandle handle) const noexcept
{
    return m_Textures.Backing(handle);
}

GraphicsResourceBacking DiligentGraphicsBackend::GetBufferBackingForTesting(
    BufferHandle handle) const noexcept
{
    return m_Buffers.Backing(handle);
}

GraphicsResourceQueryResult DiligentGraphicsBackend::QueryTextureForTesting(
    TextureHandle handle) const noexcept
{
    return m_Textures.Query(handle, GraphicsResourceKind::Texture);
}

GraphicsResourceQueryResult DiligentGraphicsBackend::QueryBufferForTesting(
    BufferHandle handle) const noexcept
{
    return m_Buffers.Query(handle, GraphicsResourceKind::Buffer);
}

GraphicsResourceQueryResult DiligentGraphicsBackend::QueryShaderForTesting(
    ShaderHandle handle) const noexcept
{
    return m_Shaders.Query(handle, GraphicsResourceKind::Shader);
}

GraphicsResourceQueryResult DiligentGraphicsBackend::QueryPipelineForTesting(
    PipelineHandle handle) const noexcept
{
    return m_Pipelines.Query(handle, GraphicsResourceKind::Pipeline);
}

GraphicsResourceQueryResult DiligentGraphicsBackend::QuerySamplerForTesting(
    SamplerHandle handle) const noexcept
{
    return m_Samplers.Query(handle, GraphicsResourceKind::Sampler);
}

bool DiligentGraphicsBackend::HasNativeDeviceServicesForTesting()
    const noexcept
{
    return m_DeviceServices.IsBound();
}

void DiligentGraphicsBackend::BindNativeDeviceServicesForTesting(
    RenderBackend::DiligentDeviceServices services,
    bool enableNativeCreation) noexcept
{
    m_DeviceServices = services;
    m_NativeCreationEnabled = enableNativeCreation && services.IsBound();
    if (m_NativeOwner)
    {
        m_NativeOwner->Bind(services);
    }
}

bool DiligentGraphicsBackend::MarkBufferNativeForTesting(
    BufferHandle handle,
    std::uint64_t nativeSerial) noexcept
{
    if (nativeSerial == 0u)
    {
        return false;
    }

    return m_Buffers.UpdateNativeState(
        handle,
        GraphicsResourceBacking::NativeGpu,
        {nativeSerial, false},
        GraphicsResourceLifecycle::Ready);
}

::Diligent::IBuffer* DiligentGraphicsBackend::ResolveNativeBufferForTesting(
    BufferHandle handle) const noexcept
{
    const auto query = m_Buffers.Query(handle, GraphicsResourceKind::Buffer);
    if (!query.valid || !query.nativeBacked || !m_NativeOwner)
    {
        return nullptr;
    }

    return m_NativeOwner->ResolveBuffer(handle);
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
    return m_RenderBackend && !m_SurfaceMinimized &&
           m_LastStatus.initialized &&
           m_LastStatus.failure == RenderBackendFailure::None;
}

bool DiligentGraphicsBackend::CanCreateResources() const noexcept
{
    return m_LastStatus.initialized &&
           m_LastStatus.failure == RenderBackendFailure::None;
}

GraphicsResourceBacking DiligentGraphicsBackend::ResourceBackingForNativeResource()
    const noexcept
{
    if (m_Headless || !m_DeviceServices.IsBound())
    {
        return GraphicsResourceBacking::RegisteredOnly;
    }

    return GraphicsResourceBacking::NativeGpuFuture;
}

GraphicsResourceLifecycle DiligentGraphicsBackend::ResourceLifecycleForBacking(
    GraphicsResourceBacking backing) const noexcept
{
    if (backing == GraphicsResourceBacking::NativeGpu ||
        backing == GraphicsResourceBacking::NativeGpuFuture)
    {
        return GraphicsResourceLifecycle::Ready;
    }

    if (backing == GraphicsResourceBacking::RegisteredOnly)
    {
        return GraphicsResourceLifecycle::RegisteredOnly;
    }

    return GraphicsResourceLifecycle::Invalid;
}

DiligentGraphicsBackend::NativeResourceOwnership
DiligentGraphicsBackend::MakeNativeResourceOwnership(
    bool uploadDeferred) noexcept
{
    if (ResourceBackingForNativeResource() !=
        GraphicsResourceBacking::NativeGpuFuture)
    {
        return {};
    }

    return {m_NextNativeResourceSerial++, uploadDeferred};
}

template <typename HandleT>
HandleT DiligentGraphicsBackend::RecordCreateFailure(
    GraphicsResourceFailure failure) noexcept
{
    m_LastResourceFailure = failure;
    ++m_FailedCreateCount;
    return {};
}

template <typename HandleT>
HandleT DiligentGraphicsBackend::RecordSuccessfulCreate(HandleT handle) noexcept
{
    m_LastResourceFailure = GraphicsResourceFailure::None;
    const auto report = BuildResourceMemoryReport();
    if (report.totalLiveBytes > m_PeakLiveBytes)
    {
        m_PeakLiveBytes = report.totalLiveBytes;
    }
    return handle;
}

GraphicsResourceMemoryReport DiligentGraphicsBackend::BuildResourceMemoryReport()
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

constexpr GraphicsResourceLeakSummary DiligentGraphicsBackend::BuildLeakSummary(
    const GraphicsResourceMemoryReport& report) noexcept
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

void DiligentGraphicsBackend::SetFailure(
    RenderBackendFailure failure) noexcept
{
    m_LastStatus.initialized = false;
    m_LastStatus.health = RenderBackendHealth::Failed;
    m_LastStatus.failure = failure;
    m_LastCapabilities = MakeConservativeCapabilities();
}

void DiligentGraphicsBackend::ReleaseResources() noexcept
{
    if (m_NativeOwner)
    {
        m_NativeOwner->ReleaseAll();
    }
    m_Textures.ReleaseAll();
    m_Buffers.ReleaseAll();
    m_Shaders.ReleaseAll();
    m_Pipelines.ReleaseAll();
    m_Samplers.ReleaseAll();
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
