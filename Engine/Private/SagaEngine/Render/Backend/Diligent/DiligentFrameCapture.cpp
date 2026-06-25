/// @file DiligentFrameCapture.cpp
/// @brief Diligent current-back-buffer capture implementation.

#include "SagaEngine/Render/Backend/Diligent/DiligentFrameCapture.h"

#include "DeviceContext.h"
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Texture.h"

#include <cstdint>
#include <vector>

namespace SagaEngine::Render::Backend
{

namespace
{

bool IsCaptureFormatSupported(Diligent::TEXTURE_FORMAT format) noexcept
{
    using namespace Diligent;
    switch (format)
    {
        case TEX_FORMAT_RGBA8_UNORM:
        case TEX_FORMAT_RGBA8_UNORM_SRGB:
        case TEX_FORMAT_BGRA8_UNORM:
        case TEX_FORMAT_BGRA8_UNORM_SRGB:
            return true;
        default:
            return false;
    }
}

void CopyMappedTextureToRGBA8(
    const Diligent::MappedTextureSubresource& mapped,
    Diligent::TEXTURE_FORMAT sourceFormat,
    std::uint32_t width,
    std::uint32_t height,
    std::vector<std::uint8_t>& outPixels)
{
    outPixels.assign(static_cast<std::size_t>(width) * height * 4u, 0u);

    const auto* srcBase = static_cast<const std::uint8_t*>(mapped.pData);
    const bool sourceIsBGRA =
        sourceFormat == Diligent::TEX_FORMAT_BGRA8_UNORM ||
        sourceFormat == Diligent::TEX_FORMAT_BGRA8_UNORM_SRGB;

    for (std::uint32_t y = 0; y < height; ++y)
    {
        const auto* src =
            srcBase + static_cast<std::size_t>(mapped.Stride) * y;
        auto* dst =
            outPixels.data() + static_cast<std::size_t>(width) * y * 4u;

        for (std::uint32_t x = 0; x < width; ++x)
        {
            const auto* px = src + static_cast<std::size_t>(x) * 4u;
            auto* out = dst + static_cast<std::size_t>(x) * 4u;
            if (sourceIsBGRA)
            {
                out[0] = px[2];
                out[1] = px[1];
                out[2] = px[0];
                out[3] = px[3];
            }
            else
            {
                out[0] = px[0];
                out[1] = px[1];
                out[2] = px[2];
                out[3] = px[3];
            }
        }
    }
}

} // namespace

RenderCaptureResult DiligentFrameCapture::Capture(
    DiligentDeviceServices services,
    RenderFrameCapture& outCapture)
{
    outCapture = {};

    if (!services.IsBound())
    {
        return RenderCaptureResult::kNotInitialized;
    }

    using namespace Diligent;

    auto* rtv = services.SwapChain()->GetCurrentBackBufferRTV();
    if (!rtv)
    {
        return RenderCaptureResult::kBackBufferUnavailable;
    }

    auto* backBuffer = rtv->GetTexture();
    if (!backBuffer)
    {
        return RenderCaptureResult::kBackBufferUnavailable;
    }

    const auto& scDesc = services.SwapChain()->GetDesc();
    if (scDesc.Width == 0 || scDesc.Height == 0 ||
        !IsCaptureFormatSupported(scDesc.ColorBufferFormat))
    {
        return RenderCaptureResult::kUnsupportedFormat;
    }

    if (m_stagingTexture)
    {
        const auto& stagingDesc = m_stagingTexture->GetDesc();
        if (stagingDesc.Width != scDesc.Width ||
            stagingDesc.Height != scDesc.Height ||
            stagingDesc.Format != scDesc.ColorBufferFormat)
        {
            m_stagingTexture.Release();
        }
    }

    if (!m_stagingTexture)
    {
        TextureDesc textureDesc;
        textureDesc.Name = "SagaFrameCaptureStagingTexture";
        textureDesc.Type = RESOURCE_DIM_TEX_2D;
        textureDesc.Width = scDesc.Width;
        textureDesc.Height = scDesc.Height;
        textureDesc.Format = scDesc.ColorBufferFormat;
        textureDesc.Usage = USAGE_STAGING;
        textureDesc.CPUAccessFlags = CPU_ACCESS_READ;
        textureDesc.MipLevels = 1;
        services.Device()->CreateTexture(
            textureDesc, nullptr, &m_stagingTexture);
        if (!m_stagingTexture)
        {
            return RenderCaptureResult::kCopyFailed;
        }
    }

    services.ImmediateContext()->SetRenderTargets(
        0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

    CopyTextureAttribs copyAttribs(
        backBuffer,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        m_stagingTexture,
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    services.ImmediateContext()->CopyTexture(copyAttribs);
    services.ImmediateContext()->WaitForIdle();

    MappedTextureSubresource mapped{};
    services.ImmediateContext()->MapTextureSubresource(
        m_stagingTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT,
        nullptr, mapped);

    if (!mapped.pData || mapped.Stride == 0)
    {
        return RenderCaptureResult::kCopyFailed;
    }

    outCapture.width = scDesc.Width;
    outCapture.height = scDesc.Height;
    outCapture.rowPitch = scDesc.Width * 4u;
    outCapture.format = RenderPixelFormat::RGBA8_UNORM;
    CopyMappedTextureToRGBA8(
        mapped,
        scDesc.ColorBufferFormat,
        scDesc.Width,
        scDesc.Height,
        outCapture.pixels);

    services.ImmediateContext()->UnmapTextureSubresource(
        m_stagingTexture, 0, 0);

    return RenderCaptureResult::kSuccess;
}

void DiligentFrameCapture::Reset()
{
    m_stagingTexture.Release();
}

} // namespace SagaEngine::Render::Backend
