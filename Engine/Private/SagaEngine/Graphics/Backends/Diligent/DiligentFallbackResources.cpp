/// @file DiligentFallbackResources.cpp
/// @brief Private native binding fallback resource ownership for Diligent.

#include "SagaEngine/Graphics/Backends/Diligent/DiligentFallbackResources.h"

#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendValidation.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

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
    ::SagaEngine::Render::Backend::DiligentNativeResourceOwner& owner,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    ++diagnostics.fallbackInitializationAttempts;
    if (m_Live)
    {
        if (owner.ResolveTextureSrv(m_WhiteTextureToken) &&
            owner.ResolveSampler(m_MaterialSamplerToken))
        {
            ++diagnostics.fallbackInitializationSuccesses;
            PublishDiagnostics(diagnostics);
            return true;
        }

        Release(owner, diagnostics);
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
    const auto textureToken = owner.AllocateToken(
        ::SagaEngine::Render::Backend::DiligentNativeResourceKind::Texture);
    const auto textureSerial = owner.CreateTextureForToken(
        textureToken,
        textureDesc,
        WhiteTextureData(kWhitePixel));
    if (textureSerial == 0u || !owner.ResolveTextureSrv(textureToken))
    {
        m_Initializing = false;
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }
    ++diagnostics.fallbackTextureCreates;

    if (m_ForceNextSamplerFailure)
    {
        m_ForceNextSamplerFailure = false;
        owner.DestroyTexture(textureToken);
        m_Initializing = false;
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }

    const auto samplerDesc = MakeMaterialSamplerDesc();
    const auto samplerToken = owner.AllocateToken(
        ::SagaEngine::Render::Backend::DiligentNativeResourceKind::Sampler);
    const auto samplerSerial = owner.CreateSamplerForToken(
        samplerToken,
        samplerDesc);
    if (samplerSerial == 0u || !owner.ResolveSampler(samplerToken))
    {
        owner.DestroyTexture(textureToken);
        m_Initializing = false;
        ++diagnostics.fallbackInitializationFailures;
        PublishDiagnostics(diagnostics);
        return false;
    }
    ++diagnostics.fallbackSamplerCreates;

    m_WhiteTextureToken = textureToken;
    m_MaterialSamplerToken = samplerToken;
    m_WhiteTexture.index = textureToken.index;
    m_WhiteTexture.generation = textureToken.generation;
    m_MaterialSampler.index = samplerToken.index;
    m_MaterialSampler.generation = samplerToken.generation;
    m_WhiteTextureCreationSerial = textureSerial;
    m_MaterialSamplerCreationSerial = samplerSerial;
    m_Live = true;
    ++m_Generation;
    m_Initializing = false;
    ++diagnostics.fallbackInitializationSuccesses;
    PublishDiagnostics(diagnostics);
    return true;
}

void DiligentFallbackResources::Release(
    ::SagaEngine::Render::Backend::DiligentNativeResourceOwner& owner,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    if (!m_Live)
    {
        PublishDiagnostics(diagnostics);
        return;
    }

    const auto sampler = m_MaterialSamplerToken;
    const auto texture = m_WhiteTextureToken;
    ResetPublishedState();
    ++m_Generation;
    ++diagnostics.fallbackReleaseCount;

    if (sampler.IsValid())
    {
        owner.DestroySampler(sampler);
    }
    if (texture.IsValid())
    {
        owner.DestroyTexture(texture);
    }

    PublishDiagnostics(diagnostics);
}

DiligentFallbackResourceIdentity DiligentFallbackResources::ResolveWhiteTexture(
    ::SagaEngine::Render::Backend::DiligentNativeResourceOwner& owner,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    DiligentFallbackResourceIdentity identity{};
    if (!Initialize(owner, diagnostics))
    {
        return identity;
    }

    identity.kind = GraphicsResourceKind::Texture;
    identity.handle = m_WhiteTexture;
    identity.nativeToken = m_WhiteTextureToken;
    identity.textureView = owner.ResolveTextureSrv(m_WhiteTextureToken);
    identity.creationSerial = m_WhiteTextureCreationSerial;
    identity.fallbackGeneration = m_Generation;
    identity.valid = identity.textureView != nullptr;
    return identity;
}

DiligentFallbackResourceIdentity
DiligentFallbackResources::ResolveMaterialSampler(
    ::SagaEngine::Render::Backend::DiligentNativeResourceOwner& owner,
    DiligentNativeBindingDiagnostics& diagnostics) noexcept
{
    DiligentFallbackResourceIdentity identity{};
    if (!Initialize(owner, diagnostics))
    {
        return identity;
    }

    identity.kind = GraphicsResourceKind::Sampler;
    identity.handle = m_MaterialSampler;
    identity.nativeToken = m_MaterialSamplerToken;
    identity.sampler = owner.ResolveSampler(m_MaterialSamplerToken);
    identity.creationSerial = m_MaterialSamplerCreationSerial;
    identity.fallbackGeneration = m_Generation;
    identity.valid = identity.sampler != nullptr;
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
    m_WhiteTextureToken = {};
    m_MaterialSamplerToken = {};
    m_WhiteTextureCreationSerial = 0;
    m_MaterialSamplerCreationSerial = 0;
    m_Live = false;
}

} // namespace SagaEngine::Graphics::Backends::Diligent
