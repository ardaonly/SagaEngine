/// @file DiligentGraphicsBackendResources.cpp
/// @brief Resource create/destroy implementation for the Diligent graphics adapter.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingCache.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendValidation.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"

#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

namespace SagaEngine::Graphics::Backends::Diligent
{

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
        const bool updated = m_Textures.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
            GraphicsResourceLifecycle::Ready);
        (void)updated;
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
        const bool updated = m_Buffers.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
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
        const auto serial = m_NativeOwner->CreateShaderForHandle(handle, desc);
        if (serial == 0u)
        {
            m_Shaders.Destroy(handle);
            return RecordCreateFailure<ShaderHandle>(
                GraphicsResourceFailure::InvalidShaderDesc);
        }
        const bool updated = m_Shaders.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
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
        const auto serial =
            m_NativeOwner->CreatePipelineForHandle(handle, storedDesc);
        if (serial == 0u)
        {
            m_Pipelines.Destroy(handle);
            return RecordCreateFailure<PipelineHandle>(
                GraphicsResourceFailure::InvalidPipelineDesc);
        }
        const bool updated = m_Pipelines.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
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
        const auto serial = m_NativeOwner->CreateSamplerForHandle(handle, desc);
        if (serial == 0u)
        {
            m_Samplers.Destroy(handle);
            return RecordCreateFailure<SamplerHandle>(
                GraphicsResourceFailure::InvalidSamplerDesc);
        }
        const bool updated = m_Samplers.UpdateNativeState(
            handle,
            GraphicsResourceBacking::NativeGpu,
            {serial, false},
            GraphicsResourceLifecycle::Ready);
        (void)updated;
    }

    return RecordSuccessfulCreate(handle);
}

void DiligentGraphicsBackend::DestroyTexture(TextureHandle handle)
{
    m_NativeBindingCache->InvalidateResource(
        GraphicsResourceKind::Texture,
        handle,
        DiligentBindingFailureReason::StaleResource,
        m_NativeBindingDiagnostics);
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyTexture(handle);
    }
    m_Textures.Destroy(handle);
}

void DiligentGraphicsBackend::DestroyBuffer(BufferHandle handle)
{
    m_NativeBindingCache->InvalidateResource(
        GraphicsResourceKind::Buffer,
        handle,
        DiligentBindingFailureReason::StaleResource,
        m_NativeBindingDiagnostics);
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
    m_NativeBindingCache->InvalidatePipeline(
        handle,
        DiligentBindingFailureReason::MissingPipeline,
        m_NativeBindingDiagnostics);
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroyPipeline(handle);
    }
    m_Pipelines.Destroy(handle);
}

void DiligentGraphicsBackend::DestroySampler(SamplerHandle handle)
{
    m_NativeBindingCache->InvalidateResource(
        GraphicsResourceKind::Sampler,
        handle,
        DiligentBindingFailureReason::StaleResource,
        m_NativeBindingDiagnostics);
    if (m_NativeOwner)
    {
        m_NativeOwner->DestroySampler(handle);
    }
    m_Samplers.Destroy(handle);
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

void DiligentGraphicsBackend::ReleaseResources() noexcept
{
    if (m_NativeBindingCache)
    {
        m_NativeBindingCache->Clear(m_NativeBindingDiagnostics);
    }
    if (m_NativeOwner)
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
