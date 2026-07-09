/// @file DiligentGraphicsBackendResources.cpp
/// @brief Resource create/destroy implementation for the Diligent graphics adapter.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"
#include "SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentBindingCache.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentFallbackResources.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendValidation.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentGpuTimeline.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"
namespace SagaEngine::Graphics::Backends::Diligent {

TextureHandle DiligentGraphicsBackend::CreateTexture(const TextureDesc& desc) { return CreateTexture(desc, {}); }

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
        const auto token = m_NativeOwner->AllocateToken(
            RenderBackend::DiligentNativeResourceKind::Texture);
        const auto serial = m_NativeOwner->CreateTextureForToken(
            token, desc, initialData);
        if (serial == 0u)
        {
            m_Textures.Destroy(handle);
            return RecordCreateFailure<TextureHandle>(
                GraphicsResourceFailure::InvalidTextureDesc);
        }
        const bool updated = m_Textures.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {token, serial, false},
            GraphicsResourceLifecycle::Ready);
        (void)updated;
    }

    return RecordSuccessfulCreate(handle);
}

BufferHandle DiligentGraphicsBackend::CreateBuffer(const BufferDesc& desc) { return CreateBuffer(desc, {}); }

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
        const auto token = m_NativeOwner->AllocateToken(
            RenderBackend::DiligentNativeResourceKind::Buffer);
        const auto serial =
            m_NativeOwner->CreateBufferForToken(token, desc, initialData);
        if (serial == 0u)
        {
            m_Buffers.Destroy(handle);
            return RecordCreateFailure<BufferHandle>(
                GraphicsResourceFailure::InvalidBufferDesc);
        }
        const bool updated = m_Buffers.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {token, serial, false},
            GraphicsResourceLifecycle::Ready);
        (void)updated;
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
        const auto token = m_NativeOwner->AllocateToken(
            RenderBackend::DiligentNativeResourceKind::Shader);
        const auto serial = m_NativeOwner->CreateShaderForToken(token, desc);
        if (serial == 0u)
        {
            m_Shaders.Destroy(handle);
            return RecordCreateFailure<ShaderHandle>(
                GraphicsResourceFailure::InvalidShaderDesc);
        }
        const bool updated = m_Shaders.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {token, serial, false},
            GraphicsResourceLifecycle::Ready);
        (void)updated;
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
            if (!ValidateGraphicsBindingLayout(desc.bindingCompatibilityLayout)
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

    const bool createNative =
        m_NativeCreationEnabled && m_NativeOwner &&
        m_NativeOwner->CanCreateNative();
    auto handle = m_Pipelines.Create(
        storedDesc,
        EstimatePipelineBytes(storedDesc),
        {},
        GraphicsResourceBacking::RegisteredOnly,
        {},
        m_NextResourceCreationSerial++,
        GraphicsResourceLifecycle::RegisteredOnly);

    if (createNative)
    {
        const auto pipelineToken = m_NativeOwner->AllocateToken(
            RenderBackend::DiligentNativeResourceKind::Pipeline);
        const auto vertexShaderToken = m_Shaders.NativeToken(storedDesc.vertexShader);
        const auto fragmentShaderToken = m_Shaders.NativeToken(storedDesc.fragmentShader);
        const auto serial = m_NativeOwner->CreatePipelineForToken(
            pipelineToken,
            vertexShaderToken,
            fragmentShaderToken,
            storedDesc);
        if (serial == 0u)
        {
            m_Pipelines.Destroy(handle);
            return RecordCreateFailure<PipelineHandle>(
                GraphicsResourceFailure::InvalidPipelineDesc);
        }
        const bool updated = m_Pipelines.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {pipelineToken, serial, false},
            GraphicsResourceLifecycle::Ready);
        (void)updated;
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
        const auto token = m_NativeOwner->AllocateToken(
            RenderBackend::DiligentNativeResourceKind::Sampler);
        const auto serial = m_NativeOwner->CreateSamplerForToken(token, desc);
        if (serial == 0u)
        {
            m_Samplers.Destroy(handle);
            return RecordCreateFailure<SamplerHandle>(
                GraphicsResourceFailure::InvalidSamplerDesc);
        }
        const bool updated = m_Samplers.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {token, serial, false},
            GraphicsResourceLifecycle::Ready);
        (void)updated;
    }

    return RecordSuccessfulCreate(handle);
}

void DiligentGraphicsBackend::DestroyTexture(TextureHandle handle)
{
    m_Runtime->BindingCache().InvalidateResource(
        GraphicsResourceKind::Texture,
        handle,
        m_Runtime->DeferredReleaseSerial(),
        DiligentBindingFailureReason::StaleResource,
        m_NativeBindingDiagnostics);
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyTexture(m_Textures.NativeToken(handle));
    }
    m_Textures.Destroy(handle);
}

void DiligentGraphicsBackend::DestroyBuffer(BufferHandle handle)
{
    m_Runtime->BindingCache().InvalidateResource(
        GraphicsResourceKind::Buffer,
        handle,
        m_Runtime->DeferredReleaseSerial(),
        DiligentBindingFailureReason::StaleResource,
        m_NativeBindingDiagnostics);
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyBuffer(m_Buffers.NativeToken(handle));
    }
    m_Buffers.Destroy(handle);
}

void DiligentGraphicsBackend::DestroyShader(ShaderHandle handle)
{
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyShader(m_Shaders.NativeToken(handle));
    }
    m_Shaders.Destroy(handle);
}

void DiligentGraphicsBackend::DestroyPipeline(PipelineHandle handle)
{
    m_Runtime->BindingCache().InvalidatePipeline(
        handle,
        m_Runtime->DeferredReleaseSerial(),
        DiligentBindingFailureReason::MissingPipeline,
        m_NativeBindingDiagnostics);
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyPipeline(m_Pipelines.NativeToken(handle));
    }
    m_Pipelines.Destroy(handle);
}

void DiligentGraphicsBackend::DestroySampler(SamplerHandle handle)
{
    m_Runtime->BindingCache().InvalidateResource(
        GraphicsResourceKind::Sampler,
        handle,
        m_Runtime->DeferredReleaseSerial(),
        DiligentBindingFailureReason::StaleResource,
        m_NativeBindingDiagnostics);
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroySampler(m_Samplers.NativeToken(handle));
    }
    m_Samplers.Destroy(handle);
}

bool DiligentGraphicsBackend::CanCreateResources() const noexcept
{
    return m_LastStatus.initialized &&
           m_LastStatus.failure == RenderBackendFailure::None;
}

GraphicsResourceBacking
DiligentGraphicsBackend::ResourceBackingForNativeResource() const noexcept
{
    if (m_Headless || !m_NativeOwner || !m_NativeOwner->CanCreateNative())
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

    return {{}, m_NextNativeResourceSerial++, uploadDeferred};
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

void DiligentGraphicsBackend::ReleaseResources() noexcept
{
    if (m_Runtime)
    {
        m_Runtime->FallbackResources().Release(m_Runtime->NativeResources(), m_NativeBindingDiagnostics);
    }
    m_Runtime->BindingCache().Clear(m_Runtime->DeferredReleaseSerial(), m_NativeBindingDiagnostics);
    m_Runtime->RetireCompleted(m_Runtime->Timeline().LastCompletedSerial());
    if (m_NativeOwner && !m_ExternalRuntime)
    {
        m_NativeOwner->ReleaseAll();
    }
    m_NativeBindingSets.clear();
    m_CompiledBindingLayouts.clear();
    m_Textures.ReleaseAll();
    m_Buffers.ReleaseAll();
    m_Shaders.ReleaseAll();
    m_Pipelines.ReleaseAll();
    m_Samplers.ReleaseAll();
    m_BindingSets.ReleaseAll();
    m_BindingLayouts.ReleaseAll();
}

} // namespace SagaEngine::Graphics::Backends::Diligent
