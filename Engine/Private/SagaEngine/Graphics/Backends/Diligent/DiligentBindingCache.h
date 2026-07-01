/// @file DiligentBindingCache.h
/// @brief Private native Diligent SRB cache for Saga binding sets.

#pragma once

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingResolver.h"

#include "RefCntAutoPtr.hpp"
#include "ShaderResourceBinding.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Diligent
{
struct IDeviceObject;
struct IPipelineState;
struct IShaderResourceBinding;
struct IShaderResourceVariable;
} // namespace Diligent

namespace SagaEngine::Graphics::Backends::Diligent
{

struct DiligentNativeBindingResourceIdentity
{
    GraphicsBindingStableId stableId = 0;
    std::uint32_t slot = 0;
    std::uint32_t arrayElement = 0;
    GraphicsResourceKind kind = GraphicsResourceKind::Invalid;
    std::uint32_t handleIndex = 0;
    std::uint32_t handleGeneration = 0;
    std::uint64_t creationSerial = 0;
};

struct DiligentNativeBindingCacheKey
{
    std::uint32_t pipelineIndex = 0;
    std::uint32_t pipelineGeneration = 0;
    std::uint64_t pipelineCreationSerial = 0;
    std::uint32_t layoutIndex = 0;
    std::uint32_t layoutGeneration = 0;
    std::uint32_t bindingSetIndex = 0;
    std::uint32_t bindingSetGeneration = 0;
    std::uint64_t compatibilityKey = 0;
    std::uint64_t fallbackGeneration = 0;
    std::vector<DiligentNativeBindingResourceIdentity> resources{};
};

[[nodiscard]] bool operator==(
    const DiligentNativeBindingResourceIdentity& lhs,
    const DiligentNativeBindingResourceIdentity& rhs) noexcept;

[[nodiscard]] bool operator==(
    const DiligentNativeBindingCacheKey& lhs,
    const DiligentNativeBindingCacheKey& rhs) noexcept;

struct DiligentNativeBindingCacheKeyHash
{
    [[nodiscard]] std::size_t operator()(
        const DiligentNativeBindingCacheKey& key) const noexcept;
};

struct DiligentCompiledVariableBinding
{
    GraphicsBindingStableId stableId = 0;
    GraphicsBindingType logicalType = GraphicsBindingType::Texture;
    std::uint32_t shaderStage = 0;
    std::string shaderVariableName;
    std::uint32_t expectedArrayCount = 1;
    GraphicsResourceKind expectedResourceKind = GraphicsResourceKind::Invalid;
    ::Diligent::IShaderResourceVariable* variable = nullptr;
    std::uint8_t diligentVariableType = 0;
    bool optional = false;
    bool usedFallback = false;
    std::uint32_t diagnosticIndex = 0;
};

struct DiligentNativeBindingCacheEntry
{
    DiligentNativeBindingCacheKey key{};
    GraphicsBindingLayoutDesc canonicalLayout{};
    ::Diligent::RefCntAutoPtr<::Diligent::IShaderResourceBinding> srb{};
    std::vector<DiligentCompiledVariableBinding> variables{};
    std::uint64_t creationSerial = 0;
    std::uint64_t hitCount = 0;
    std::uint64_t missCreateCount = 0;
    bool valid = false;
    DiligentBindingFailureReason invalidationReason =
        DiligentBindingFailureReason::None;
    std::string debugName;
};

struct DiligentBindingCacheResolveResult
{
    ::Diligent::IShaderResourceBinding* srb = nullptr;
    DiligentBindingFailureReason failure =
        DiligentBindingFailureReason::None;
    bool success = false;
    bool cacheHit = false;
};

class DiligentBindingCache final
{
public:
    [[nodiscard]] DiligentBindingCacheResolveResult ResolveOrCreate(
        ::Diligent::IPipelineState& pipeline,
        const DiligentResolvedBindingSet& resolved,
        DiligentNativeBindingDiagnostics& diagnostics);

    void InvalidatePipeline(
        PipelineHandle handle,
        DiligentBindingFailureReason reason,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;
    void InvalidateLayout(
        BindingLayoutHandle handle,
        DiligentBindingFailureReason reason,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;
    void InvalidateBindingSet(
        BindingSetHandle handle,
        DiligentBindingFailureReason reason,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;
    void InvalidateResource(
        GraphicsResourceKind kind,
        GraphicsHandle handle,
        DiligentBindingFailureReason reason,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;
    void Clear(DiligentNativeBindingDiagnostics& diagnostics) noexcept;

    [[nodiscard]] std::uint64_t EntryCount() const noexcept;
    [[nodiscard]] std::uint64_t QuarantinedCount() const noexcept;

private:
    using EntryMap = std::unordered_map<
        DiligentNativeBindingCacheKey,
        DiligentNativeBindingCacheEntry,
        DiligentNativeBindingCacheKeyHash>;

    void Quarantine(
        DiligentNativeBindingCacheEntry& entry,
        DiligentBindingFailureReason reason,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;

    EntryMap m_Entries;
    std::vector<::Diligent::RefCntAutoPtr<::Diligent::IShaderResourceBinding>>
        m_QuarantinedSrbs;
    std::uint64_t m_NextCreationSerial = 1;
};

} // namespace SagaEngine::Graphics::Backends::Diligent
