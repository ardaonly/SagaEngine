/// @file DiligentFallbackResources.h
/// @brief Private native binding fallback resources for the Diligent adapter.

#pragma once

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingRecords.h"
#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceToken.h"

#include <cstdint>

namespace Diligent
{
struct ITextureView;
struct ISampler;
} // namespace Diligent

namespace SagaEngine::Render::Backend
{
class DiligentNativeResourceOwner;
} // namespace SagaEngine::Render::Backend

namespace SagaEngine::Graphics::Backends::Diligent
{

struct DiligentFallbackResourceIdentity
{
    GraphicsResourceKind kind = GraphicsResourceKind::Invalid;
    GraphicsHandle handle{};
    ::SagaEngine::Render::Backend::DiligentNativeResourceToken nativeToken{};
    // Private TODO: callers should eventually request runtime-owned
    // fallback binding operations instead of carrying raw native payloads.
    ::Diligent::ITextureView* textureView = nullptr;
    ::Diligent::ISampler* sampler = nullptr;
    std::uint64_t creationSerial = 0;
    std::uint64_t fallbackGeneration = 0;
    bool valid = false;
};

class DiligentFallbackResources final
{
public:
    [[nodiscard]] bool Initialize(
        ::SagaEngine::Render::Backend::DiligentNativeResourceOwner& owner,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;
    void Release(
        ::SagaEngine::Render::Backend::DiligentNativeResourceOwner& owner,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;

    [[nodiscard]] DiligentFallbackResourceIdentity ResolveWhiteTexture(
        ::SagaEngine::Render::Backend::DiligentNativeResourceOwner& owner,
        DiligentNativeBindingDiagnostics& diagnostics) noexcept;
    [[nodiscard]] DiligentFallbackResourceIdentity ResolveMaterialSampler(
        ::SagaEngine::Render::Backend::DiligentNativeResourceOwner& owner,
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
    ::SagaEngine::Render::Backend::DiligentNativeTextureToken
        m_WhiteTextureToken{};
    ::SagaEngine::Render::Backend::DiligentNativeSamplerToken
        m_MaterialSamplerToken{};
    std::uint64_t m_WhiteTextureCreationSerial = 0;
    std::uint64_t m_MaterialSamplerCreationSerial = 0;
    std::uint64_t m_Generation = 0;
    bool m_Live = false;
    bool m_Initializing = false;
    bool m_ForceNextSamplerFailure = false;
};

} // namespace SagaEngine::Graphics::Backends::Diligent
