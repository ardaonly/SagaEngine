/// @file DiligentBindingResolver.cpp
/// @brief Private native binding resource resolver for the Diligent adapter.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingCache.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingResolver.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendValidation.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

#include "Buffer.h"
#include "DeviceObject.h"
#include "Sampler.h"
#include "TextureView.h"

#include <utility>

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace
{

[[nodiscard]] const DiligentCompiledBindingEntry* FindEntry(
    const DiligentCompiledBindingLayout& layout,
    const DiligentNativeBindingResourceRecord& resource) noexcept
{
    for (const auto& entry : layout.entries)
    {
        if (resource.stableId != 0u && entry.stableId == resource.stableId)
        {
            return &entry;
        }
        if (resource.stableId == 0u && entry.slot == resource.slot)
        {
            return &entry;
        }
    }
    return nullptr;
}

void AddDiagnostic(
    DiligentResolvedBindingSet& resolved,
    DiligentBindingFailureReason reason,
    const char* message)
{
    resolved.failure = reason;
    resolved.valid = false;
    resolved.diagnostics.push_back(message);
}

void CountFailure(
    DiligentNativeBindingDiagnostics& diagnostics,
    DiligentBindingFailureReason reason) noexcept
{
    ++diagnostics.nativeBindingResolveFailures;
    switch (reason)
    {
    case DiligentBindingFailureReason::MissingLayout:
        ++diagnostics.staleLayoutRejects;
        break;
    case DiligentBindingFailureReason::MissingBindingSet:
        ++diagnostics.staleBindingSetRejects;
        break;
    case DiligentBindingFailureReason::MissingPipeline:
        ++diagnostics.stalePipelineRejects;
        break;
    case DiligentBindingFailureReason::CanonicalLayoutMismatch:
    case DiligentBindingFailureReason::PipelineIncompatible:
        ++diagnostics.canonicalLayoutMismatchRejects;
        break;
    case DiligentBindingFailureReason::ResourceKindMismatch:
        ++diagnostics.resourceKindMismatchRejects;
        break;
    case DiligentBindingFailureReason::StaleResource:
    case DiligentBindingFailureReason::ResourceGenerationMismatch:
    case DiligentBindingFailureReason::NativePayloadMissing:
        ++diagnostics.staleResourceRejects;
        break;
    case DiligentBindingFailureReason::RequiredBindingMissing:
    case DiligentBindingFailureReason::FallbackRequired:
        ++diagnostics.requiredBindingMissingRejects;
        break;
    case DiligentBindingFailureReason::UnsupportedBinding:
    case DiligentBindingFailureReason::UnsupportedArrayBinding:
    case DiligentBindingFailureReason::UnsupportedBufferRange:
        ++diagnostics.unsupportedBindingRejects;
        break;
    default:
        break;
    }
}

[[nodiscard]] bool IsSampledTextureDesc(const TextureDesc& desc) noexcept
{
    return HasFlag(desc.usage, TextureUsageFlags::Sampled);
}

} // namespace

DiligentResolvedBindingSet DiligentGraphicsBackend::ResolveNativeBindingSet(
    PipelineHandle pipeline,
    BindingSetHandle bindingSet) noexcept
{
    ++m_NativeBindingDiagnostics.nativeBindingResolveAttempts;

    DiligentResolvedBindingSet resolved{};
    resolved.pipeline = pipeline;
    resolved.bindingSet = bindingSet;

    const auto bindingSetKey =
        PackHandleKey(bindingSet.index, bindingSet.generation);
    const auto bindingSetIt = m_NativeBindingSets.find(bindingSetKey);
    const auto* bindingSetDesc = m_BindingSets.ResolveDesc(bindingSet);
    if (bindingSetIt == m_NativeBindingSets.end() || !bindingSetDesc)
    {
        AddDiagnostic(
            resolved,
            DiligentBindingFailureReason::MissingBindingSet,
            "binding set is not live");
        CountFailure(m_NativeBindingDiagnostics, resolved.failure);
        return resolved;
    }

    const auto& nativeSet = bindingSetIt->second;
    resolved.layout = bindingSetDesc->layout;

    const auto layoutKey =
        PackHandleKey(resolved.layout.index, resolved.layout.generation);
    const auto layoutIt = m_CompiledBindingLayouts.find(layoutKey);
    const auto* layoutDesc = m_BindingLayouts.ResolveDesc(resolved.layout);
    if (layoutIt == m_CompiledBindingLayouts.end() || !layoutDesc)
    {
        AddDiagnostic(
            resolved,
            DiligentBindingFailureReason::MissingLayout,
            "binding layout is not live");
        CountFailure(m_NativeBindingDiagnostics, resolved.failure);
        return resolved;
    }

    const auto& compiledLayout = layoutIt->second;
    if (nativeSet.layoutHandleGeneration != resolved.layout.generation ||
        nativeSet.compatibilityKey != compiledLayout.compatibilityKey)
    {
        AddDiagnostic(
            resolved,
            DiligentBindingFailureReason::CanonicalLayoutMismatch,
            "binding set layout generation/key no longer matches layout");
        CountFailure(m_NativeBindingDiagnostics, resolved.failure);
        return resolved;
    }

    if (!AreGraphicsBindingLayoutsCanonicallyEqual(
            nativeSet.canonicalLayout,
            compiledLayout.canonicalLayout))
    {
        AddDiagnostic(
            resolved,
            DiligentBindingFailureReason::CanonicalLayoutMismatch,
            "binding set canonical layout no longer matches compiled layout");
        CountFailure(m_NativeBindingDiagnostics, resolved.failure);
        return resolved;
    }

    const auto pipelineQuery = QueryPipelineForTesting(pipeline);
    const auto* pipelineDesc = m_Pipelines.ResolveDesc(pipeline);
    if (!pipelineQuery.live || !pipelineQuery.nativeBacked ||
        !pipelineDesc || !m_NativeOwner ||
        !m_NativeOwner->ResolvePipeline(pipeline))
    {
        AddDiagnostic(
            resolved,
            DiligentBindingFailureReason::MissingPipeline,
            "pipeline is not live native GPU state");
        CountFailure(m_NativeBindingDiagnostics, resolved.failure);
        return resolved;
    }

    if (pipelineDesc->bindingCompatibilityKey !=
            compiledLayout.compatibilityKey ||
        !AreGraphicsBindingLayoutsCanonicallyEqual(
            pipelineDesc->bindingCompatibilityLayout,
            compiledLayout.canonicalLayout))
    {
        AddDiagnostic(
            resolved,
            DiligentBindingFailureReason::PipelineIncompatible,
            "pipeline binding layout is not compatible with binding set");
        CountFailure(m_NativeBindingDiagnostics, resolved.failure);
        return resolved;
    }

    if (!nativeSet.fallbackRequirements.empty())
    {
        AddDiagnostic(
            resolved,
            DiligentBindingFailureReason::FallbackRequired,
            "binding set requires fallback resources not implemented in B2");
        CountFailure(m_NativeBindingDiagnostics, resolved.failure);
        return resolved;
    }

    resolved.pipelineCreationSerial = pipelineQuery.creationSerial;
    resolved.compatibilityKey = compiledLayout.compatibilityKey;
    resolved.canonicalLayout = compiledLayout.canonicalLayout;

    for (const auto& resource : nativeSet.resources)
    {
        const auto* entry = FindEntry(compiledLayout, resource);
        if (!entry)
        {
            AddDiagnostic(
                resolved,
                DiligentBindingFailureReason::ResourceKindMismatch,
                "binding resource no longer has a compiled entry");
            CountFailure(m_NativeBindingDiagnostics, resolved.failure);
            return resolved;
        }

        if (entry->arrayCount != 1u || resource.arrayElement != 0u)
        {
            AddDiagnostic(
                resolved,
                DiligentBindingFailureReason::UnsupportedArrayBinding,
                "array bindings are not supported in B2");
            CountFailure(m_NativeBindingDiagnostics, resolved.failure);
            return resolved;
        }

        DiligentResolvedBindingResource resolvedResource{};
        resolvedResource.entry = entry;
        resolvedResource.record = resource;
        resolvedResource.query = QueryResource(resource.kind, resource.handle);
        if (!resolvedResource.query.live ||
            resolvedResource.query.kind != resource.kind)
        {
            AddDiagnostic(
                resolved,
                DiligentBindingFailureReason::StaleResource,
                "binding resource is stale");
            CountFailure(m_NativeBindingDiagnostics, resolved.failure);
            return resolved;
        }

        if (resolvedResource.query.creationSerial !=
            resource.resourceCreationSerial)
        {
            AddDiagnostic(
                resolved,
                DiligentBindingFailureReason::ResourceGenerationMismatch,
                "binding resource creation serial changed");
            CountFailure(m_NativeBindingDiagnostics, resolved.failure);
            return resolved;
        }

        if (!resolvedResource.query.nativeBacked || !m_NativeOwner)
        {
            AddDiagnostic(
                resolved,
                DiligentBindingFailureReason::NativePayloadMissing,
                "binding resource has no native payload");
            CountFailure(m_NativeBindingDiagnostics, resolved.failure);
            return resolved;
        }

        switch (entry->kind)
        {
        case DiligentCompiledBindingKind::SampledTexture:
        {
            if (resource.kind != GraphicsResourceKind::Texture)
            {
                AddDiagnostic(
                    resolved,
                    DiligentBindingFailureReason::ResourceKindMismatch,
                    "sampled texture binding received a non-texture resource");
                CountFailure(m_NativeBindingDiagnostics, resolved.failure);
                return resolved;
            }
            const auto textureHandle =
                ToTypedHandle<TextureHandle>(resource.handle);
            const auto* textureDesc = m_Textures.ResolveDesc(textureHandle);
            auto* srv = m_NativeOwner->ResolveTextureSrv(textureHandle);
            if (!textureDesc || !IsSampledTextureDesc(*textureDesc) || !srv)
            {
                AddDiagnostic(
                    resolved,
                    DiligentBindingFailureReason::NativePayloadMissing,
                    "sampled texture has no valid SRV");
                CountFailure(m_NativeBindingDiagnostics, resolved.failure);
                return resolved;
            }
            resolvedResource.textureView = srv;
            resolvedResource.nativeObject =
                static_cast<::Diligent::IDeviceObject*>(srv);
            break;
        }
        case DiligentCompiledBindingKind::Sampler:
        {
            if (resource.kind != GraphicsResourceKind::Sampler)
            {
                AddDiagnostic(
                    resolved,
                    DiligentBindingFailureReason::ResourceKindMismatch,
                    "sampler binding received a non-sampler resource");
                CountFailure(m_NativeBindingDiagnostics, resolved.failure);
                return resolved;
            }
            const auto samplerHandle =
                ToTypedHandle<SamplerHandle>(resource.handle);
            auto* sampler = m_NativeOwner->ResolveSampler(samplerHandle);
            if (!sampler)
            {
                AddDiagnostic(
                    resolved,
                    DiligentBindingFailureReason::NativePayloadMissing,
                    "sampler has no native payload");
                CountFailure(m_NativeBindingDiagnostics, resolved.failure);
                return resolved;
            }
            resolvedResource.sampler = sampler;
            resolvedResource.nativeObject =
                static_cast<::Diligent::IDeviceObject*>(sampler);
            break;
        }
        case DiligentCompiledBindingKind::ConstantBuffer:
        {
            if (resource.kind != GraphicsResourceKind::Buffer)
            {
                AddDiagnostic(
                    resolved,
                    DiligentBindingFailureReason::ResourceKindMismatch,
                    "constant buffer binding received a non-buffer resource");
                CountFailure(m_NativeBindingDiagnostics, resolved.failure);
                return resolved;
            }
            if (resource.bufferOffsetBytes != 0u ||
                resource.bufferRangeBytes != 0u)
            {
                AddDiagnostic(
                    resolved,
                    DiligentBindingFailureReason::UnsupportedBufferRange,
                    "buffer subranges are not supported in B2");
                CountFailure(m_NativeBindingDiagnostics, resolved.failure);
                return resolved;
            }
            const auto bufferHandle =
                ToTypedHandle<BufferHandle>(resource.handle);
            const auto* bufferDesc = m_Buffers.ResolveDesc(bufferHandle);
            auto* buffer = m_NativeOwner->ResolveBuffer(bufferHandle);
            if (!bufferDesc || bufferDesc->usage != BufferUsage::Uniform ||
                !buffer)
            {
                AddDiagnostic(
                    resolved,
                    DiligentBindingFailureReason::NativePayloadMissing,
                    "constant buffer has no valid native uniform buffer");
                CountFailure(m_NativeBindingDiagnostics, resolved.failure);
                return resolved;
            }
            resolvedResource.buffer = buffer;
            resolvedResource.nativeObject =
                static_cast<::Diligent::IDeviceObject*>(buffer);
            break;
        }
        case DiligentCompiledBindingKind::Unknown:
        default:
            AddDiagnostic(
                resolved,
                DiligentBindingFailureReason::UnsupportedBinding,
                "unsupported binding kind reached native resolver");
            CountFailure(m_NativeBindingDiagnostics, resolved.failure);
            return resolved;
        }

        resolvedResource.valid = true;
        resolved.resources.push_back(std::move(resolvedResource));
    }

    resolved.valid = true;
    resolved.failure = DiligentBindingFailureReason::None;
    ++m_NativeBindingDiagnostics.nativeBindingResolveSuccesses;
    return resolved;
}

DiligentBindingCacheResolveResult
DiligentGraphicsBackend::ResolveNativeBindingSrb(
    PipelineHandle pipeline,
    BindingSetHandle bindingSet) noexcept
{
    const auto resolved = ResolveNativeBindingSet(pipeline, bindingSet);
    if (!resolved.valid || !m_NativeOwner)
    {
        return {nullptr, resolved.failure, false, false};
    }

    auto* nativePipeline = m_NativeOwner->ResolvePipeline(pipeline);
    if (!nativePipeline)
    {
        ++m_NativeBindingDiagnostics.nativeBindingResolveFailures;
        ++m_NativeBindingDiagnostics.stalePipelineRejects;
        return {
            nullptr,
            DiligentBindingFailureReason::MissingPipeline,
            false,
            false,
        };
    }

    auto result = m_NativeBindingCache->ResolveOrCreate(
        *nativePipeline,
        resolved,
        m_NativeBindingDiagnostics);
    if (!result.success)
    {
        ++m_NativeBindingDiagnostics.nativeBindingResolveFailures;
    }
    return result;
}

} // namespace SagaEngine::Graphics::Backends::Diligent
