/// @file DiligentFallbackResources.cpp
/// @brief Private native binding fallback resource ownership for Diligent.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentFallbackResources.h"

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendValidation.h"

#include <array>

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace
{

[[nodiscard]] TextureDesc MakeWhiteTextureDesc()
{
    TextureDesc desc{};
    desc.debugName = "Saga.Internal.Fallback.WhiteTexture";
    desc.width = 1u;
    desc.height = 1u;
    desc.depth = 1u;
    desc.mipLevels = 1u;
    desc.arrayLayers = 1u;
    desc.format = ResourceFormat::Rgba8Unorm;
    desc.dimension = TextureDimension::Texture2D;
    desc.usage = TextureUsageFlags::Sampled;
    return desc;
}

[[nodiscard]] SamplerDesc MakeMaterialSamplerDesc()
{
    SamplerDesc desc{};
    desc.debugName = "Saga.Internal.Fallback.MaterialSampler";
    desc.minFilter = FilterMode::Linear;
    desc.magFilter = FilterMode::Linear;
    desc.addressU = AddressMode::Repeat;
    desc.addressV = AddressMode::Repeat;
    desc.addressW = AddressMode::Repeat;
    desc.mipLodBias = 0.0f;
    return desc;
}

[[nodiscard]] GraphicsDataView WhiteTextureData(
    const std::array<std::uint8_t, 4>& pixel) noexcept
{
    return {
        pixel.data(),
        static_cast<std::uint64_t>(pixel.size()),
        4u,
        0u,
    };
}

} // namespace

bool DiligentFallbackResources::Initialize(
    DiligentGraphicsBackend& backend,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    ++diagnostics.fallbackInitializationAttempts;
    if (m_Live)
    {
        const auto textureQuery = backend.QueryTextureForTesting(m_WhiteTexture);
        const auto samplerQuery = backend.QuerySamplerForTesting(m_MaterialSampler);
        if (textureQuery.live && textureQuery.nativeBacked &&
            samplerQuery.live && samplerQuery.nativeBacked &&
            backend.ResolveNativeTextureSrvForTesting(m_WhiteTexture) &&
            backend.ResolveNativeSamplerForTesting(m_MaterialSampler))
        {
            ++diagnostics.fallbackInitializationSuccesses;
            PublishDiagnostics(diagnostics);
            return true;
        }

        Release(backend, diagnostics);
    }

    if (m_Initializing)
    {
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }
    m_Initializing = true;

    static constexpr std::array<std::uint8_t, 4> kWhitePixel{
        255u,
        255u,
        255u,
        255u,
    };

    const auto textureDesc = MakeWhiteTextureDesc();
    const auto texture =
        backend.CreateTexture(textureDesc, WhiteTextureData(kWhitePixel));
    if (!texture.IsValid())
    {
        m_Initializing = false;
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }
    ++diagnostics.fallbackTextureCreates;

    const auto textureQuery = backend.QueryTextureForTesting(texture);
    if (!textureQuery.live || !textureQuery.nativeBacked ||
        !backend.ResolveNativeTextureSrvForTesting(texture))
    {
        backend.DestroyTexture(texture);
        m_Initializing = false;
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }

    if (m_ForceNextSamplerFailure)
    {
        m_ForceNextSamplerFailure = false;
        backend.DestroyTexture(texture);
        m_Initializing = false;
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }

    const auto samplerDesc = MakeMaterialSamplerDesc();
    const auto sampler = backend.CreateSampler(samplerDesc);
    if (!sampler.IsValid())
    {
        backend.DestroyTexture(texture);
        m_Initializing = false;
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }
    ++diagnostics.fallbackSamplerCreates;

    const auto samplerQuery = backend.QuerySamplerForTesting(sampler);
    if (!samplerQuery.live || !samplerQuery.nativeBacked ||
        !backend.ResolveNativeSamplerForTesting(sampler))
    {
        backend.DestroySampler(sampler);
        backend.DestroyTexture(texture);
        m_Initializing = false;
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }

    m_WhiteTexture = texture;
    m_MaterialSampler = sampler;
    m_WhiteTextureCreationSerial = textureQuery.creationSerial;
    m_MaterialSamplerCreationSerial = samplerQuery.creationSerial;
    m_Live = true;
    ++m_Generation;
    m_Initializing = false;
    ++diagnostics.fallbackInitializationSuccesses;
    PublishDiagnostics(diagnostics);
    return true;
}

void DiligentFallbackResources::Release(
    DiligentGraphicsBackend& backend,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    if (!m_Live)
    {
        PublishDiagnostics(diagnostics);
        return;
    }

    const auto sampler = m_MaterialSampler;
    const auto texture = m_WhiteTexture;
    ResetPublishedState();
    ++m_Generation;
    ++diagnostics.fallbackReleaseCount;

    if (sampler.IsValid())
    {
        backend.DestroySampler(sampler);
    }
    if (texture.IsValid())
    {
        backend.DestroyTexture(texture);
    }

    PublishDiagnostics(diagnostics);
}

DiligentFallbackResourceIdentity DiligentFallbackResources::ResolveWhiteTexture(
    DiligentGraphicsBackend& backend,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    DiligentFallbackResourceIdentity identity{};
    if (!Initialize(backend, diagnostics))
    {
        return identity;
    }

    identity.kind = GraphicsResourceKind::Texture;
    identity.handle = m_WhiteTexture;
    identity.creationSerial = m_WhiteTextureCreationSerial;
    identity.fallbackGeneration = m_Generation;
    identity.valid = true;
    return identity;
}

DiligentFallbackResourceIdentity
DiligentFallbackResources::ResolveMaterialSampler(
    DiligentGraphicsBackend& backend,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    DiligentFallbackResourceIdentity identity{};
    if (!Initialize(backend, diagnostics))
    {
        return identity;
    }

    identity.kind = GraphicsResourceKind::Sampler;
    identity.handle = m_MaterialSampler;
    identity.creationSerial = m_MaterialSamplerCreationSerial;
    identity.fallbackGeneration = m_Generation;
    identity.valid = true;
    return identity;
}

TextureHandle DiligentFallbackResources::WhiteTexture() const noexcept
{
    return m_WhiteTexture;
}

SamplerHandle DiligentFallbackResources::MaterialSampler() const noexcept
{
    return m_MaterialSampler;
}

std::uint64_t DiligentFallbackResources::Generation() const noexcept
{
    return m_Generation;
}

bool DiligentFallbackResources::Live() const noexcept
{
    return m_Live;
}

std::uint32_t DiligentFallbackResources::InternalResourceCount() const noexcept
{
    return m_Live ? 2u : 0u;
}

bool DiligentFallbackResources::IsInternalTexture(
    TextureHandle handle) const noexcept
{
    return m_Live && handle.index == m_WhiteTexture.index &&
           handle.generation == m_WhiteTexture.generation;
}

bool DiligentFallbackResources::IsInternalSampler(
    SamplerHandle handle) const noexcept
{
    return m_Live && handle.index == m_MaterialSampler.index &&
           handle.generation == m_MaterialSampler.generation;
}

std::uint64_t DiligentFallbackResources::InternalTextureBytes() const noexcept
{
    return m_Live ? EstimateTextureBytes(MakeWhiteTextureDesc()) : 0u;
}

void DiligentFallbackResources::ForceNextSamplerFailureForTesting(
    bool enabled) noexcept
{
    m_ForceNextSamplerFailure = enabled;
}

void DiligentFallbackResources::PublishDiagnostics(
    DiligentNativeBindingDiagnostics& diagnostics) const noexcept
{
    diagnostics.fallbackGeneration = m_Generation;
    diagnostics.fallbackLiveState = m_Live;
    diagnostics.fallbackInternalResourceCount = InternalResourceCount();
}

void DiligentFallbackResources::ResetPublishedState() noexcept
{
    m_WhiteTexture = {};
    m_MaterialSampler = {};
    m_WhiteTextureCreationSerial = 0;
    m_MaterialSamplerCreationSerial = 0;
    m_Live = false;
}

} // namespace SagaEngine::Graphics::Backends::Diligent
