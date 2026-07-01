/// @file DiligentBindingCache.cpp
/// @brief Private native Diligent SRB cache for Saga binding sets.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingCache.h"

#include "DeviceObject.h"
#include "PipelineState.h"
#include "ShaderResourceBinding.h"
#include "ShaderResourceVariable.h"

#include <algorithm>
#include <array>
#include <utility>

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace
{

void HashAppend(std::size_t& hash, std::uint64_t value) noexcept
{
    constexpr std::size_t prime = 1099511628211ull;
    for (std::uint32_t i = 0; i < 8u; ++i)
    {
        const auto byte =
            static_cast<std::uint8_t>((value >> (i * 8u)) & 0xFFu);
        hash ^= byte;
        hash *= prime;
    }
}

[[nodiscard]] std::array<std::uint32_t, 3> StagesForMask(
    std::uint32_t mask) noexcept
{
    std::array<std::uint32_t, 3> stages{};
    std::uint32_t count = 0;
    if ((mask & ::Diligent::SHADER_TYPE_VERTEX) != 0u)
    {
        stages[count++] = ::Diligent::SHADER_TYPE_VERTEX;
    }
    if ((mask & ::Diligent::SHADER_TYPE_PIXEL) != 0u)
    {
        stages[count++] = ::Diligent::SHADER_TYPE_PIXEL;
    }
    if ((mask & ::Diligent::SHADER_TYPE_COMPUTE) != 0u)
    {
        stages[count++] = ::Diligent::SHADER_TYPE_COMPUTE;
    }
    return stages;
}

[[nodiscard]] DiligentNativeBindingCacheKey MakeCacheKey(
    const DiligentResolvedBindingSet& resolved)
{
    DiligentNativeBindingCacheKey key{};
    key.pipelineIndex = resolved.pipeline.index;
    key.pipelineGeneration = resolved.pipeline.generation;
    key.pipelineCreationSerial = resolved.pipelineCreationSerial;
    key.layoutIndex = resolved.layout.index;
    key.layoutGeneration = resolved.layout.generation;
    key.bindingSetIndex = resolved.bindingSet.index;
    key.bindingSetGeneration = resolved.bindingSet.generation;
    key.compatibilityKey = resolved.compatibilityKey;

    key.resources.reserve(resolved.resources.size());
    for (const auto& resource : resolved.resources)
    {
        DiligentNativeBindingResourceIdentity identity{};
        identity.stableId = resource.record.stableId;
        identity.slot = resource.record.slot;
        identity.arrayElement = resource.record.arrayElement;
        identity.kind = resource.record.kind;
        identity.handleIndex = resource.record.handle.index;
        identity.handleGeneration = resource.record.handle.generation;
        identity.creationSerial = resource.record.resourceCreationSerial;
        key.resources.push_back(identity);
    }

    std::sort(
        key.resources.begin(),
        key.resources.end(),
        [](const auto& lhs, const auto& rhs)
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
    return key;
}

[[nodiscard]] bool IsStaticVariable(
    const DiligentCompiledBindingEntry& entry) noexcept
{
    return entry.diligentVariableType ==
           static_cast<std::uint8_t>(
               ::Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
}

[[nodiscard]] bool CompileVariableBinding(
    const DiligentResolvedBindingResource& resource,
    std::uint32_t stage,
    ::Diligent::IShaderResourceVariable& variable,
    DiligentNativeBindingCacheEntry& pending)
{
    variable.Set(resource.nativeObject);

    DiligentCompiledVariableBinding compiled{};
    compiled.stableId = resource.entry->stableId;
    compiled.logicalType = resource.entry->bindingType;
    compiled.shaderStage = stage;
    compiled.shaderVariableName = resource.entry->shaderVariableName;
    compiled.expectedArrayCount = resource.entry->arrayCount;
    compiled.expectedResourceKind = resource.record.kind;
    compiled.variable = &variable;
    compiled.diligentVariableType = resource.entry->diligentVariableType;
    compiled.optional = !resource.entry->required;
    compiled.usedFallback = resource.usedFallback;
    compiled.diagnosticIndex =
        static_cast<std::uint32_t>(pending.variables.size());
    pending.variables.push_back(std::move(compiled));
    return true;
}

} // namespace

bool operator==(
    const DiligentNativeBindingResourceIdentity& lhs,
    const DiligentNativeBindingResourceIdentity& rhs) noexcept
{
    return lhs.stableId == rhs.stableId && lhs.slot == rhs.slot &&
           lhs.arrayElement == rhs.arrayElement && lhs.kind == rhs.kind &&
           lhs.handleIndex == rhs.handleIndex &&
           lhs.handleGeneration == rhs.handleGeneration &&
           lhs.creationSerial == rhs.creationSerial;
}

bool operator==(
    const DiligentNativeBindingCacheKey& lhs,
    const DiligentNativeBindingCacheKey& rhs) noexcept
{
    return lhs.pipelineIndex == rhs.pipelineIndex &&
           lhs.pipelineGeneration == rhs.pipelineGeneration &&
           lhs.pipelineCreationSerial == rhs.pipelineCreationSerial &&
           lhs.layoutIndex == rhs.layoutIndex &&
           lhs.layoutGeneration == rhs.layoutGeneration &&
           lhs.bindingSetIndex == rhs.bindingSetIndex &&
           lhs.bindingSetGeneration == rhs.bindingSetGeneration &&
           lhs.compatibilityKey == rhs.compatibilityKey &&
           lhs.fallbackGeneration == rhs.fallbackGeneration &&
           lhs.resources == rhs.resources;
}

std::size_t DiligentNativeBindingCacheKeyHash::operator()(
    const DiligentNativeBindingCacheKey& key) const noexcept
{
    std::size_t hash = 14695981039346656037ull;
    HashAppend(hash, key.pipelineIndex);
    HashAppend(hash, key.pipelineGeneration);
    HashAppend(hash, key.pipelineCreationSerial);
    HashAppend(hash, key.layoutIndex);
    HashAppend(hash, key.layoutGeneration);
    HashAppend(hash, key.bindingSetIndex);
    HashAppend(hash, key.bindingSetGeneration);
    HashAppend(hash, key.compatibilityKey);
    HashAppend(hash, key.fallbackGeneration);
    for (const auto& resource : key.resources)
    {
        HashAppend(hash, resource.stableId);
        HashAppend(hash, resource.slot);
        HashAppend(hash, resource.arrayElement);
        HashAppend(hash, static_cast<std::uint64_t>(resource.kind));
        HashAppend(hash, resource.handleIndex);
        HashAppend(hash, resource.handleGeneration);
        HashAppend(hash, resource.creationSerial);
    }
    return hash;
}

DiligentBindingCacheResolveResult DiligentBindingCache::ResolveOrCreate(
    ::Diligent::IPipelineState& pipeline,
    const DiligentResolvedBindingSet& resolved,
    DiligentNativeBindingDiagnostics& diagnostics)
{
    DiligentBindingCacheResolveResult result{};
    if (!resolved.valid)
    {
        result.failure = resolved.failure;
        return result;
    }

    const auto key = MakeCacheKey(resolved);
    auto it = m_Entries.find(key);
    if (it != m_Entries.end() && it->second.valid && it->second.srb)
    {
        ++diagnostics.nativeBindingCacheHits;
        ++it->second.hitCount;
        result.srb = it->second.srb.RawPtr();
        result.success = true;
        result.cacheHit = true;
        return result;
    }

    ++diagnostics.nativeBindingCacheMisses;

    ::Diligent::RefCntAutoPtr<::Diligent::IShaderResourceBinding> srb;
    DiligentNativeBindingCacheEntry pending{};
    pending.key = key;
    pending.canonicalLayout = resolved.canonicalLayout;
    pending.creationSerial = m_NextCreationSerial++;
    pending.missCreateCount = 1;
    pending.valid = true;

    for (const auto& resource : resolved.resources)
    {
        if (!resource.valid || !resource.entry ||
            resource.entry->shaderVariableName.empty())
        {
            ++diagnostics.nativeBindingVariableLookupFailures;
            result.failure = DiligentBindingFailureReason::VariableLookupFailed;
            return result;
        }

        const auto stages = StagesForMask(resource.entry->diligentStageMask);
        for (const auto stage : stages)
        {
            if (stage == 0u)
            {
                continue;
            }

            if (!IsStaticVariable(*resource.entry))
            {
                continue;
            }

            ++diagnostics.nativeBindingVariableLookups;
            auto* variable = pipeline.GetStaticVariableByName(
                static_cast<::Diligent::SHADER_TYPE>(stage),
                resource.entry->shaderVariableName.c_str());
            if (!variable)
            {
                ++diagnostics.nativeBindingVariableLookupFailures;
                result.failure =
                    DiligentBindingFailureReason::VariableLookupFailed;
                return result;
            }

            (void)CompileVariableBinding(resource, stage, *variable, pending);
        }
    }

    pipeline.CreateShaderResourceBinding(&srb, true);
    if (!srb)
    {
        ++diagnostics.nativeBindingSrbCreateFailures;
        result.failure = DiligentBindingFailureReason::SrbCreationFailed;
        return result;
    }
    ++diagnostics.nativeBindingSrbCreates;
    pending.srb = srb;

    for (const auto& resource : resolved.resources)
    {
        if (!resource.valid || !resource.entry ||
            resource.entry->shaderVariableName.empty())
        {
            ++diagnostics.nativeBindingVariableLookupFailures;
            result.failure = DiligentBindingFailureReason::VariableLookupFailed;
            return result;
        }

        if (IsStaticVariable(*resource.entry))
        {
            continue;
        }

        const auto stages = StagesForMask(resource.entry->diligentStageMask);
        for (const auto stage : stages)
        {
            if (stage == 0u)
            {
                continue;
            }

            ++diagnostics.nativeBindingVariableLookups;
            auto* variable = srb->GetVariableByName(
                static_cast<::Diligent::SHADER_TYPE>(stage),
                resource.entry->shaderVariableName.c_str());
            if (!variable)
            {
                ++diagnostics.nativeBindingVariableLookupFailures;
                result.failure =
                    DiligentBindingFailureReason::VariableLookupFailed;
                return result;
            }

            (void)CompileVariableBinding(resource, stage, *variable, pending);
        }
    }

    auto [inserted, _] = m_Entries.emplace(key, std::move(pending));
    diagnostics.nativeBindingCacheEntries =
        static_cast<std::uint64_t>(m_Entries.size());
    result.srb = inserted->second.srb.RawPtr();
    result.success = true;
    return result;
}

void DiligentBindingCache::Quarantine(
    DiligentNativeBindingCacheEntry& entry,
    DiligentBindingFailureReason reason,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    entry.valid = false;
    entry.invalidationReason = reason;
    if (entry.srb)
    {
        m_QuarantinedSrbs.push_back(entry.srb);
        diagnostics.quarantinedSrbCount =
            static_cast<std::uint64_t>(m_QuarantinedSrbs.size());
        diagnostics.quarantinedSrbPeak = std::max(
            diagnostics.quarantinedSrbPeak,
            diagnostics.quarantinedSrbCount);
    }
}

void DiligentBindingCache::InvalidatePipeline(
    PipelineHandle handle,
    DiligentBindingFailureReason reason,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    for (auto it = m_Entries.begin(); it != m_Entries.end();)
    {
        if (it->first.pipelineIndex == handle.index &&
            it->first.pipelineGeneration == handle.generation)
        {
            Quarantine(it->second, reason, diagnostics);
            it = m_Entries.erase(it);
            ++diagnostics.nativeBindingCacheInvalidations;
            continue;
        }
        ++it;
    }
    diagnostics.nativeBindingCacheEntries =
        static_cast<std::uint64_t>(m_Entries.size());
}

void DiligentBindingCache::InvalidateLayout(
    BindingLayoutHandle handle,
    DiligentBindingFailureReason reason,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    for (auto it = m_Entries.begin(); it != m_Entries.end();)
    {
        if (it->first.layoutIndex == handle.index &&
            it->first.layoutGeneration == handle.generation)
        {
            Quarantine(it->second, reason, diagnostics);
            it = m_Entries.erase(it);
            ++diagnostics.nativeBindingCacheInvalidations;
            continue;
        }
        ++it;
    }
    diagnostics.nativeBindingCacheEntries =
        static_cast<std::uint64_t>(m_Entries.size());
}

void DiligentBindingCache::InvalidateBindingSet(
    BindingSetHandle handle,
    DiligentBindingFailureReason reason,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    for (auto it = m_Entries.begin(); it != m_Entries.end();)
    {
        if (it->first.bindingSetIndex == handle.index &&
            it->first.bindingSetGeneration == handle.generation)
        {
            Quarantine(it->second, reason, diagnostics);
            it = m_Entries.erase(it);
            ++diagnostics.nativeBindingCacheInvalidations;
            continue;
        }
        ++it;
    }
    diagnostics.nativeBindingCacheEntries =
        static_cast<std::uint64_t>(m_Entries.size());
}

void DiligentBindingCache::InvalidateResource(
    GraphicsResourceKind kind,
    GraphicsHandle handle,
    DiligentBindingFailureReason reason,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    for (auto it = m_Entries.begin(); it != m_Entries.end();)
    {
        const auto& resources = it->first.resources;
        const bool matches = std::any_of(
            resources.begin(),
            resources.end(),
            [kind, handle](const auto& resource)
            {
                return resource.kind == kind &&
                       resource.handleIndex == handle.index &&
                       resource.handleGeneration == handle.generation;
            });
        if (matches)
        {
            Quarantine(it->second, reason, diagnostics);
            it = m_Entries.erase(it);
            ++diagnostics.nativeBindingCacheInvalidations;
            continue;
        }
        ++it;
    }
    diagnostics.nativeBindingCacheEntries =
        static_cast<std::uint64_t>(m_Entries.size());
}

void DiligentBindingCache::Clear(
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    m_Entries.clear();
    m_QuarantinedSrbs.clear();
    diagnostics.nativeBindingCacheEntries = 0;
    diagnostics.quarantinedSrbCount = 0;
}

std::uint64_t DiligentBindingCache::EntryCount() const noexcept
{
    return static_cast<std::uint64_t>(m_Entries.size());
}

std::uint64_t DiligentBindingCache::QuarantinedCount() const noexcept
{
    return static_cast<std::uint64_t>(m_QuarantinedSrbs.size());
}

} // namespace SagaEngine::Graphics::Backends::Diligent
