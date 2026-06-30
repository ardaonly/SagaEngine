/// @file DiligentBindingCompiler.cpp
/// @brief Private binding metadata compiler for the Diligent adapter.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingCompiler.h"

#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace
{

constexpr std::uint32_t kDiligentShaderTypeVertex = 0x0001u;
constexpr std::uint32_t kDiligentShaderTypePixel = 0x0002u;
constexpr std::uint32_t kDiligentShaderTypeCompute = 0x0020u;
constexpr std::uint8_t kDiligentVariableTypeStatic = 0u;
constexpr std::uint8_t kDiligentVariableTypeMutable = 1u;
constexpr std::uint8_t kDiligentVariableTypeDynamic = 2u;

[[nodiscard]] std::uint32_t ToDiligentStageMask(
    GraphicsShaderStageFlags stages) noexcept
{
    std::uint32_t result = 0;
    if ((stages & kGraphicsShaderStageVertex) != 0u)
    {
        result |= kDiligentShaderTypeVertex;
    }
    if ((stages & kGraphicsShaderStageFragment) != 0u)
    {
        result |= kDiligentShaderTypePixel;
    }
    if ((stages & kGraphicsShaderStageCompute) != 0u)
    {
        result |= kDiligentShaderTypeCompute;
    }
    return result;
}

[[nodiscard]] std::uint8_t ToDiligentVariableType(
    GraphicsBindingFrequency frequency) noexcept
{
    switch (frequency)
    {
    case GraphicsBindingFrequency::Static:
    case GraphicsBindingFrequency::Frame:
        return kDiligentVariableTypeStatic;
    case GraphicsBindingFrequency::Material:
        return kDiligentVariableTypeMutable;
    case GraphicsBindingFrequency::Draw:
    case GraphicsBindingFrequency::Pass:
    default:
        return kDiligentVariableTypeDynamic;
    }
}

[[nodiscard]] const GraphicsBindingLayoutSlot* FindSlotByNumber(
    const GraphicsBindingLayoutDesc& layout,
    std::uint32_t slot) noexcept
{
    for (const auto& entry : layout.slots)
    {
        if (entry.slot == slot)
        {
            return &entry;
        }
    }
    return nullptr;
}

[[nodiscard]] const DiligentCompiledBindingEntry* FindCompiledEntry(
    const DiligentCompiledBindingLayout& layout,
    const GraphicsBindingResourceRef& resource) noexcept
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

[[nodiscard]] const GraphicsBindingResourceRef* FindResourceForSlot(
    const GraphicsBindingSetDesc& bindingSet,
    const GraphicsBindingLayoutSlot& slot,
    std::uint32_t arrayElement) noexcept
{
    for (const auto& resource : bindingSet.resources)
    {
        const bool matchesStableId =
            slot.stableId != 0u && resource.stableId == slot.stableId;
        const bool matchesSlot =
            slot.stableId == 0u && resource.slot == slot.slot;
        if ((matchesStableId || matchesSlot) &&
            resource.arrayElement == arrayElement)
        {
            return &resource;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string TextureVariableName(std::uint32_t slot)
{
    if (slot == 0u)
    {
        return "g_Albedo";
    }
    if (slot == 1u)
    {
        return "g_ShadowMap";
    }
    return {};
}

[[nodiscard]] std::string SamplerVariableName(
    const GraphicsBindingLayoutDesc& layout,
    std::uint32_t slot)
{
    for (const auto& textureSlot : layout.slots)
    {
        if (textureSlot.type != GraphicsBindingType::Texture ||
            textureSlot.pairedSamplerSlot != slot)
        {
            continue;
        }

        if (textureSlot.slot == 0u)
        {
            return "g_Albedo_sampler";
        }
        if (textureSlot.slot == 1u)
        {
            return "g_ShadowMap_sampler";
        }
    }

    if (slot == 0u)
    {
        return "g_Albedo_sampler";
    }
    if (slot == 1u)
    {
        return "g_ShadowMap_sampler";
    }
    return {};
}

[[nodiscard]] std::string ConstantBufferVariableName(
    std::uint32_t slot,
    GraphicsBindingFrequency frequency)
{
    if (slot == 0u || frequency == GraphicsBindingFrequency::Frame ||
        frequency == GraphicsBindingFrequency::Draw)
    {
        return "CameraCB";
    }
    if (slot == 1u)
    {
        return "BoneCB";
    }
    return {};
}

[[nodiscard]] DiligentCompiledBindingEntry CompileEntry(
    const GraphicsBindingLayoutDesc& layout,
    const GraphicsBindingLayoutSlot& slot)
{
    DiligentCompiledBindingEntry entry{};
    entry.stableId = slot.stableId;
    entry.slot = slot.slot;
    entry.bindingType = slot.type;
    entry.stageMask = slot.stages;
    entry.arrayCount = slot.arrayCount;
    entry.frequency = slot.frequency;
    entry.required = slot.required;
    entry.fallbackPolicy = slot.fallbackPolicy;
    entry.diligentStageMask = ToDiligentStageMask(slot.stages);
    entry.diligentVariableType = ToDiligentVariableType(slot.frequency);

    switch (slot.type)
    {
    case GraphicsBindingType::Texture:
        entry.kind = DiligentCompiledBindingKind::SampledTexture;
        entry.shaderVariableName = TextureVariableName(slot.slot);
        break;
    case GraphicsBindingType::Sampler:
        entry.kind = DiligentCompiledBindingKind::Sampler;
        entry.shaderVariableName = SamplerVariableName(layout, slot.slot);
        break;
    case GraphicsBindingType::Buffer:
        entry.kind = DiligentCompiledBindingKind::ConstantBuffer;
        entry.shaderVariableName =
            ConstantBufferVariableName(slot.slot, slot.frequency);
        break;
    case GraphicsBindingType::StorageBuffer:
    default:
        entry.kind = DiligentCompiledBindingKind::Unknown;
        break;
    }

    return entry;
}

[[nodiscard]] bool OptionalUnknownAllowed(
    const GraphicsBindingLayoutSlot& slot) noexcept
{
    return !slot.required &&
           slot.fallbackPolicy != GraphicsBindingFallbackPolicy::None;
}

void AddDiagnostic(std::vector<std::string>& diagnostics, std::string message)
{
    diagnostics.push_back(std::move(message));
}

} // namespace

bool AreDiligentCompiledBindingLayoutsCompatible(
    const DiligentCompiledBindingLayout& lhs,
    const DiligentCompiledBindingLayout& rhs)
{
    return lhs.compatibilityKey == rhs.compatibilityKey &&
           AreGraphicsBindingLayoutsCanonicallyEqual(
               lhs.canonicalLayout,
               rhs.canonicalLayout);
}

DiligentCompiledBindingLayout CompileDiligentBindingLayout(
    BindingLayoutHandle handle,
    const GraphicsBindingLayoutDesc& normalizedLayout)
{
    DiligentCompiledBindingLayout compiled{};
    compiled.layoutHandleIndex = handle.index;
    compiled.layoutHandleGeneration = handle.generation;
    compiled.compatibilityKey =
        ComputeGraphicsBindingLayoutKey(normalizedLayout);
    compiled.canonicalLayout = normalizedLayout;

    const auto validation = ValidateGraphicsBindingLayout(normalizedLayout);
    if (!validation.valid)
    {
        compiled.status = DiligentBindingCompileStatus::Failed;
        AddDiagnostic(compiled.diagnostics, "layout validation failed");
        return compiled;
    }

    for (const auto& slot : normalizedLayout.slots)
    {
        auto entry = CompileEntry(normalizedLayout, slot);
        if (entry.shaderVariableName.empty())
        {
            if (!OptionalUnknownAllowed(slot))
            {
                compiled.status = DiligentBindingCompileStatus::Failed;
                AddDiagnostic(
                    compiled.diagnostics,
                    "required binding has no current Diligent shader ABI name");
                return compiled;
            }

            AddDiagnostic(
                compiled.diagnostics,
                "optional binding deferred to fallback resolution");
        }

        compiled.entries.push_back(std::move(entry));
    }

    compiled.status = DiligentBindingCompileStatus::Compiled;
    return compiled;
}

DiligentNativeBindingSetRecord CompileDiligentNativeBindingSet(
    BindingSetHandle handle,
    const GraphicsBindingSetDesc& normalizedBindingSet,
    const DiligentCompiledBindingLayout& compiledLayout,
    GraphicsBindingResourceQueryFn queryResource,
    void* userData)
{
    DiligentNativeBindingSetRecord record{};
    record.bindingSetHandleIndex = handle.index;
    record.bindingSetHandleGeneration = handle.generation;
    record.layoutHandleIndex = normalizedBindingSet.layout.index;
    record.layoutHandleGeneration = normalizedBindingSet.layout.generation;
    record.compatibilityKey = compiledLayout.compatibilityKey;
    record.canonicalLayout = compiledLayout.canonicalLayout;

    if (compiledLayout.status != DiligentBindingCompileStatus::Compiled)
    {
        record.status = DiligentBindingCompileStatus::Failed;
        AddDiagnostic(record.diagnostics, "compiled layout is not usable");
        return record;
    }

    for (const auto& resource : normalizedBindingSet.resources)
    {
        const auto* entry = FindCompiledEntry(compiledLayout, resource);
        if (!entry)
        {
            record.status = DiligentBindingCompileStatus::Failed;
            AddDiagnostic(record.diagnostics, "binding resource has no entry");
            return record;
        }

        const auto query =
            queryResource ? queryResource(userData, resource.kind, resource.handle)
                          : GraphicsResourceQueryResult{};
        if (!query.live || query.kind != resource.kind)
        {
            record.status = DiligentBindingCompileStatus::Failed;
            AddDiagnostic(record.diagnostics, "binding resource is stale");
            return record;
        }

        DiligentNativeBindingResourceRecord nativeResource{};
        nativeResource.stableId = resource.stableId;
        nativeResource.slot = resource.slot;
        nativeResource.kind = resource.kind;
        nativeResource.handle = resource.handle;
        nativeResource.arrayElement = resource.arrayElement;
        nativeResource.bufferOffsetBytes = resource.bufferOffsetBytes;
        nativeResource.bufferRangeBytes = resource.bufferRangeBytes;
        nativeResource.resourceCreationSerial = query.creationSerial;
        nativeResource.shaderVariableName = entry->shaderVariableName;
        record.resources.push_back(std::move(nativeResource));
    }

    for (const auto& slot : compiledLayout.canonicalLayout.slots)
    {
        if (slot.fallbackPolicy == GraphicsBindingFallbackPolicy::None)
        {
            continue;
        }

        for (std::uint32_t element = 0; element < slot.arrayCount; ++element)
        {
            if (FindResourceForSlot(normalizedBindingSet, slot, element))
            {
                continue;
            }

            const auto* compiledSlot =
                FindSlotByNumber(compiledLayout.canonicalLayout, slot.slot);
            const GraphicsBindingResourceRef lookup{
                slot.slot,
                slot.stableId,
                ToGraphicsResourceKind(slot.type),
                {},
                element,
            };
            const auto* entry = compiledSlot
                ? FindCompiledEntry(compiledLayout, lookup)
                : nullptr;

            DiligentNativeBindingResourceRecord fallback{};
            fallback.stableId = slot.stableId;
            fallback.slot = slot.slot;
            fallback.kind = ToGraphicsResourceKind(slot.type);
            fallback.arrayElement = element;
            fallback.fallbackRequired = true;
            if (entry)
            {
                fallback.shaderVariableName = entry->shaderVariableName;
            }
            record.fallbackRequirements.push_back(std::move(fallback));
        }
    }

    record.status = DiligentBindingCompileStatus::Compiled;
    return record;
}

} // namespace SagaEngine::Graphics::Backends::Diligent
