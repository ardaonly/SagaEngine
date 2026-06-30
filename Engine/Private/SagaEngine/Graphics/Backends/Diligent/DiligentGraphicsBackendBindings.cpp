/// @file DiligentGraphicsBackendBindings.cpp
/// @brief Logical binding layout/set CRUD for the Diligent graphics adapter.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendValidation.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"

#include <algorithm>

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace
{

[[nodiscard]] constexpr std::uint64_t EstimateBindingLayoutBytes(
    const GraphicsBindingLayoutDesc& desc) noexcept
{
    return static_cast<std::uint64_t>(desc.slots.size()) *
           sizeof(GraphicsBindingLayoutSlot);
}

[[nodiscard]] constexpr std::uint64_t EstimateBindingSetBytes(
    const GraphicsBindingSetDesc& desc) noexcept
{
    return static_cast<std::uint64_t>(desc.resources.size()) *
           sizeof(GraphicsBindingResourceRef);
}

[[nodiscard]] GraphicsResourceQueryResult QueryDiligentBindingResource(
    void* userData,
    GraphicsResourceKind kind,
    GraphicsHandle handle)
{
    return static_cast<DiligentGraphicsBackend*>(userData)->QueryResource(
        kind,
        handle);
}

[[nodiscard]] GraphicsBindingSetDesc NormalizeBindingSet(
    const GraphicsBindingSetDesc& desc)
{
    GraphicsBindingSetDesc normalized = desc;
    std::sort(
        normalized.resources.begin(),
        normalized.resources.end(),
        [](const GraphicsBindingResourceRef& lhs,
           const GraphicsBindingResourceRef& rhs)
        {
            if (lhs.slot != rhs.slot)
            {
                return lhs.slot < rhs.slot;
            }
            if (lhs.stableId != rhs.stableId)
            {
                return lhs.stableId < rhs.stableId;
            }
            return lhs.arrayElement < rhs.arrayElement;
        });
    return normalized;
}

} // namespace

BindingLayoutHandle DiligentGraphicsBackend::CreateBindingLayout(
    const GraphicsBindingLayoutDesc& desc)
{
    if (!CanCreateResources())
    {
        m_LastResourceFailure = GraphicsResourceFailure::BackendNotInitialized;
        ++m_FailedCreateCount;
        return {};
    }

    if (!ValidateGraphicsBindingLayout(desc).valid)
    {
        m_LastResourceFailure =
            GraphicsResourceFailure::InvalidBindingLayoutDesc;
        ++m_FailedCreateCount;
        return {};
    }

    const auto normalized = NormalizeGraphicsBindingLayout(desc);
    auto handle = m_BindingLayouts.Create(
        normalized,
        EstimateBindingLayoutBytes(normalized),
        {},
        GraphicsResourceBacking::RegisteredOnly,
        {},
        m_NextResourceCreationSerial++,
        GraphicsResourceLifecycle::RegisteredOnly);
    m_LastResourceFailure = GraphicsResourceFailure::None;
    return handle;
}

BindingSetHandle DiligentGraphicsBackend::CreateBindingSet(
    const GraphicsBindingSetDesc& desc)
{
    if (!CanCreateResources())
    {
        m_LastResourceFailure = GraphicsResourceFailure::BackendNotInitialized;
        ++m_FailedCreateCount;
        return {};
    }

    const auto* layoutDesc = m_BindingLayouts.ResolveDesc(desc.layout);
    if (!layoutDesc)
    {
        m_LastResourceFailure = GraphicsResourceFailure::InvalidBindingSetDesc;
        ++m_FailedCreateCount;
        return {};
    }

    if (!ValidateGraphicsBindingSet(
             *layoutDesc,
             desc,
             QueryDiligentBindingResource,
             this)
             .valid)
    {
        m_LastResourceFailure = GraphicsResourceFailure::InvalidBindingSetDesc;
        ++m_FailedCreateCount;
        return {};
    }

    const auto normalized = NormalizeBindingSet(desc);
    auto handle = m_BindingSets.Create(
        normalized,
        EstimateBindingSetBytes(normalized),
        {},
        GraphicsResourceBacking::RegisteredOnly,
        {},
        m_NextResourceCreationSerial++,
        GraphicsResourceLifecycle::RegisteredOnly);
    m_LastResourceFailure = GraphicsResourceFailure::None;
    return handle;
}

void DiligentGraphicsBackend::DestroyBindingLayout(
    BindingLayoutHandle handle)
{
    m_BindingLayouts.Destroy(handle);
}

void DiligentGraphicsBackend::DestroyBindingSet(BindingSetHandle handle)
{
    m_BindingSets.Destroy(handle);
}

GraphicsBindingLayoutQueryResult DiligentGraphicsBackend::QueryBindingLayout(
    BindingLayoutHandle handle) const
{
    const auto* desc = m_BindingLayouts.ResolveDesc(handle);
    if (!desc)
    {
        return {};
    }

    return {
        true,
        true,
        ComputeGraphicsBindingLayoutKey(*desc),
        static_cast<std::uint32_t>(desc->slots.size()),
        desc->debugName,
        *desc,
    };
}

GraphicsBindingSetQueryResult DiligentGraphicsBackend::QueryBindingSet(
    BindingSetHandle handle) const
{
    const auto* desc = m_BindingSets.ResolveDesc(handle);
    if (!desc)
    {
        return {};
    }

    const auto* layoutDesc = m_BindingLayouts.ResolveDesc(desc->layout);
    const auto layoutQuery = QueryBindingLayout(desc->layout);
    return {
        true,
        true,
        desc->layout,
        layoutQuery.compatibilityKey,
        static_cast<std::uint32_t>(desc->resources.size()),
        layoutDesc ? CountGraphicsBindingFallbackRequirements(
                         *layoutDesc,
                         *desc)
                   : 0u,
        desc->debugName,
    };
}

} // namespace SagaEngine::Graphics::Backends::Diligent
