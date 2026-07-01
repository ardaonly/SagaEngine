/// @file DiligentFallbackResources.h
/// @brief Private native binding fallback resources for the Diligent adapter.

#pragma once

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingRecords.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"

#include <cstdint>

namespace SagaEngine::Graphics::Backends::Diligent
{

class DiligentGraphicsBackend;

struct DiligentFallbackResourceIdentity
{
    GraphicsResourceKind kind = GraphicsResourceKind::Invalid;
    GraphicsHandle handle{};
    std::uint64_t creationSerial = 0;
    std::uint64_t fallbackGeneration = 0;
    bool valid = false;
};

class DiligentFallbackResources final
{
public:
    [[nodiscard]] bool Initialize(
        DiligentGraphicsBackend& backend,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;
    void Release(
        DiligentGraphicsBackend& backend,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;

    [[nodiscard]] DiligentFallbackResourceIdentity ResolveWhiteTexture(
        DiligentGraphicsBackend& backend,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;
    [[nodiscard]] DiligentFallbackResourceIdentity ResolveMaterialSampler(
        DiligentGraphicsBackend& backend,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;

    [[nodiscard]] TextureHandle WhiteTexture() const noexcept;
    [[nodiscard]] SamplerHandle MaterialSampler() const noexcept;
    [[nodiscard]] std::uint64_t Generation() const noexcept;
    [[nodiscard]] bool Live() const noexcept;
    [[nodiscard]] std::uint32_t InternalResourceCount() const noexcept;
    [[nodiscard]] bool IsInternalTexture(TextureHandle handle) const noexcept;
    [[nodiscard]] bool IsInternalSampler(SamplerHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t InternalTextureBytes() const noexcept;

    void ForceNextSamplerFailureForTesting(bool enabled) noexcept;

private:
    void PublishDiagnostics(DiligentNativeBindingDiagnostics& diagnostics)
        const noexcept;
    void ResetPublishedState() noexcept;

    TextureHandle m_WhiteTexture{};
    SamplerHandle m_MaterialSampler{};
    std::uint64_t m_WhiteTextureCreationSerial = 0;
    std::uint64_t m_MaterialSamplerCreationSerial = 0;
    std::uint64_t m_Generation = 0;
    bool m_Live = false;
    bool m_Initializing = false;
    bool m_ForceNextSamplerFailure = false;
};

} // namespace SagaEngine::Graphics::Backends::Diligent
