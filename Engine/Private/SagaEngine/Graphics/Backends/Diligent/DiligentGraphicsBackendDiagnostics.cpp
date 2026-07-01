/// @file DiligentGraphicsBackendDiagnostics.cpp
/// @brief Diagnostics, queries, and testing native resolves for the Diligent graphics adapter.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingCache.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentFallbackResources.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendValidation.h"

#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

namespace SagaEngine::Graphics::Backends::Diligent
{

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

bool DiligentGraphicsBackend::MarkTextureNativeForTesting(
    TextureHandle handle,
    std::uint64_t nativeSerial) noexcept
{
    if (nativeSerial == 0u)
    {
        return false;
    }

    return m_Textures.UpdateNativeState(
        handle,
        GraphicsResourceBacking::NativeGpu,
        {nativeSerial, false},
        GraphicsResourceLifecycle::Ready);
}

bool DiligentGraphicsBackend::MarkSamplerNativeForTesting(
    SamplerHandle handle,
    std::uint64_t nativeSerial) noexcept
{
    if (nativeSerial == 0u)
    {
        return false;
    }

    return m_Samplers.UpdateNativeState(
        handle,
        GraphicsResourceBacking::NativeGpu,
        {nativeSerial, false},
        GraphicsResourceLifecycle::Ready);
}

bool DiligentGraphicsBackend::MarkShaderNativeForTesting(
    ShaderHandle handle,
    std::uint64_t nativeSerial) noexcept
{
    if (nativeSerial == 0u)
    {
        return false;
    }

    return m_Shaders.UpdateNativeState(
        handle,
        GraphicsResourceBacking::NativeGpu,
        {nativeSerial, false},
        GraphicsResourceLifecycle::Ready);
}

bool DiligentGraphicsBackend::MarkPipelineNativeForTesting(
    PipelineHandle handle,
    std::uint64_t nativeSerial) noexcept
{
    if (nativeSerial == 0u)
    {
        return false;
    }

    return m_Pipelines.UpdateNativeState(
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

::Diligent::ITextureView*
DiligentGraphicsBackend::ResolveNativeTextureSrvForTesting(
    TextureHandle handle) const noexcept
{
    const auto query = m_Textures.Query(handle, GraphicsResourceKind::Texture);
    if (!query.valid || !query.nativeBacked || !m_NativeOwner)
    {
        return nullptr;
    }

    return m_NativeOwner->ResolveTextureSrv(handle);
}

::Diligent::ISampler* DiligentGraphicsBackend::ResolveNativeSamplerForTesting(
    SamplerHandle handle) const noexcept
{
    const auto query = m_Samplers.Query(handle, GraphicsResourceKind::Sampler);
    if (!query.valid || !query.nativeBacked || !m_NativeOwner)
    {
        return nullptr;
    }

    return m_NativeOwner->ResolveSampler(handle);
}

::Diligent::IShader* DiligentGraphicsBackend::ResolveNativeShaderForTesting(
    ShaderHandle handle) const noexcept
{
    const auto query = m_Shaders.Query(handle, GraphicsResourceKind::Shader);
    if (!query.valid || !query.nativeBacked || !m_NativeOwner)
    {
        return nullptr;
    }

    return m_NativeOwner->ResolveShader(handle);
}

::Diligent::IPipelineState*
DiligentGraphicsBackend::ResolveNativePipelineForTesting(
    PipelineHandle handle) const noexcept
{
    const auto query = m_Pipelines.Query(handle, GraphicsResourceKind::Pipeline);
    if (!query.valid || !query.nativeBacked || !m_NativeOwner)
    {
        return nullptr;
    }

    return m_NativeOwner->ResolvePipeline(handle);
}

std::uint32_t DiligentGraphicsBackend::GetCompiledBindingLayoutCountForTesting()
    const noexcept
{
    return static_cast<std::uint32_t>(m_CompiledBindingLayouts.size());
}

const DiligentCompiledBindingLayout*
DiligentGraphicsBackend::ResolveCompiledBindingLayoutForTesting(
    BindingLayoutHandle handle) const noexcept
{
    const auto it = m_CompiledBindingLayouts.find(
        PackHandleKey(handle.index, handle.generation));
    return it == m_CompiledBindingLayouts.end() ? nullptr : &it->second;
}

std::uint32_t DiligentGraphicsBackend::GetNativeBindingSetRecordCountForTesting()
    const noexcept
{
    return static_cast<std::uint32_t>(m_NativeBindingSets.size());
}

const DiligentNativeBindingSetRecord*
DiligentGraphicsBackend::ResolveNativeBindingSetRecordForTesting(
    BindingSetHandle handle) const noexcept
{
    const auto it = m_NativeBindingSets.find(
        PackHandleKey(handle.index, handle.generation));
    return it == m_NativeBindingSets.end() ? nullptr : &it->second;
}

bool DiligentGraphicsBackend::CorruptNativeBindingSetCanonicalLayoutForTesting(
    BindingSetHandle handle) noexcept
{
    const auto it = m_NativeBindingSets.find(
        PackHandleKey(handle.index, handle.generation));
    if (it == m_NativeBindingSets.end() ||
        it->second.canonicalLayout.slots.empty())
    {
        return false;
    }

    ++it->second.canonicalLayout.slots.front().arrayCount;
    return true;
}

::Diligent::IShaderResourceBinding*
DiligentGraphicsBackend::ResolveNativeBindingSrbForTesting(
    PipelineHandle pipeline,
    BindingSetHandle bindingSet) noexcept
{
    const auto result = ResolveNativeBindingSrb(pipeline, bindingSet);
    return result.success ? result.srb : nullptr;
}

DiligentNativeBindingDiagnostics
DiligentGraphicsBackend::GetNativeBindingDiagnosticsForTesting()
    const noexcept
{
    auto diagnostics = m_NativeBindingDiagnostics;
    diagnostics.nativeBindingCacheEntries =
        m_NativeBindingCache ? m_NativeBindingCache->EntryCount() : 0u;
    diagnostics.quarantinedSrbCount =
        m_NativeBindingCache ? m_NativeBindingCache->QuarantinedCount() : 0u;
    diagnostics.fallbackGeneration =
        m_FallbackResources ? m_FallbackResources->Generation() : 0u;
    diagnostics.fallbackLiveState =
        m_FallbackResources ? m_FallbackResources->Live() : false;
    diagnostics.fallbackInternalResourceCount =
        m_FallbackResources ? m_FallbackResources->InternalResourceCount() : 0u;
    return diagnostics;
}

std::uint64_t
DiligentGraphicsBackend::GetNativeBindingCacheEntryCountForTesting()
    const noexcept
{
    return m_NativeBindingCache ? m_NativeBindingCache->EntryCount() : 0u;
}

std::uint64_t
DiligentGraphicsBackend::GetNativeBindingQuarantinedSrbCountForTesting()
    const noexcept
{
    return m_NativeBindingCache ? m_NativeBindingCache->QuarantinedCount() : 0u;
}

TextureHandle DiligentGraphicsBackend::GetFallbackWhiteTextureForTesting()
    const noexcept
{
    return m_FallbackResources ? m_FallbackResources->WhiteTexture()
                               : TextureHandle{};
}

SamplerHandle DiligentGraphicsBackend::GetFallbackMaterialSamplerForTesting()
    const noexcept
{
    return m_FallbackResources ? m_FallbackResources->MaterialSampler()
                               : SamplerHandle{};
}

std::uint64_t DiligentGraphicsBackend::GetFallbackGenerationForTesting()
    const noexcept
{
    return m_FallbackResources ? m_FallbackResources->Generation() : 0u;
}

bool DiligentGraphicsBackend::InitializeFallbackResourcesForTesting() noexcept
{
    return m_FallbackResources &&
           m_FallbackResources->Initialize(*this, m_NativeBindingDiagnostics);
}

void DiligentGraphicsBackend::ReleaseFallbackResourcesForTesting() noexcept
{
    if (m_FallbackResources)
    {
        m_FallbackResources->Release(*this, m_NativeBindingDiagnostics);
    }
}

void DiligentGraphicsBackend::ForceNextFallbackSamplerFailureForTesting(
    bool enabled) noexcept
{
    if (m_FallbackResources)
    {
        m_FallbackResources->ForceNextSamplerFailureForTesting(enabled);
    }
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
    if (m_FallbackResources)
    {
        if (m_FallbackResources->Live())
        {
            if (report.liveTextureCount != 0u)
            {
                --report.liveTextureCount;
            }
            if (report.liveSamplerCount != 0u)
            {
                --report.liveSamplerCount;
            }
            const auto textureBytes =
                m_FallbackResources->InternalTextureBytes();
            report.textureBytes =
                report.textureBytes > textureBytes
                    ? report.textureBytes - textureBytes
                    : 0u;
        }
    }
    report.totalLiveBytes =
        report.textureBytes + report.bufferBytes + report.shaderBytes +
        report.pipelineBytes + report.samplerBytes;
    report.peakLiveBytes = m_PeakLiveBytes;
    report.failedCreateCount = m_FailedCreateCount;
    return report;
}

GraphicsResourceLeakSummary DiligentGraphicsBackend::BuildLeakSummary(
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

} // namespace SagaEngine::Graphics::Backends::Diligent
