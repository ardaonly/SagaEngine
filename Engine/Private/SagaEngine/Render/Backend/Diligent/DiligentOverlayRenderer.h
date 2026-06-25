/// @file DiligentOverlayRenderer.h
/// @brief Private Diligent renderer for vendor-neutral overlay draw lists.

#pragma once

#include "SagaEngine/Render/Backend/Diligent/DiligentOverlayTextureRegistry.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"

#include "RefCntAutoPtr.hpp"

#include <cstdint>
#include <thread>
#include <vector>

namespace Diligent
{
struct IBuffer;
struct IPipelineState;
struct IShaderResourceBinding;
struct IShaderResourceVariable;
struct ITexture;
struct ITextureView;
} // namespace Diligent

namespace SagaEngine::Render::Backend
{

class DiligentNativeResourceOwner;

class DiligentOverlayRenderer final
{
public:
    DiligentOverlayRenderer() = default;
    ~DiligentOverlayRenderer() = default;

    DiligentOverlayRenderer(const DiligentOverlayRenderer&) = delete;
    DiligentOverlayRenderer& operator=(const DiligentOverlayRenderer&) = delete;

    [[nodiscard]] bool Initialize(
        DiligentDeviceServices services,
        DiligentNativeResourceOwner& nativeResources,
        std::uint32_t frameSlotCount);
    void Shutdown();

    [[nodiscard]] RenderOverlayTextureHandle CreateTexture(
        const RenderOverlayTextureDesc& desc,
        const std::uint8_t* rgbaPixels);
    void DestroyTexture(RenderOverlayTextureHandle texture);

    void Render(
        const RenderOverlayFrame& frame,
        std::uint64_t activeFrameSerial,
        std::uint32_t activeFrameSlot);
    [[nodiscard]] bool ReconfigureFrameSlots(std::uint32_t frameSlotCount);

    [[nodiscard]] bool IsReady() const noexcept { return m_ready; }
    [[nodiscard]] bool IsRenderThread() const noexcept;
    [[nodiscard]] std::size_t FrameSlotCountForTests() const noexcept
    {
        return m_frameBuffers.size();
    }
    void CaptureRenderThreadForTests() noexcept
    {
        m_ownerThread = std::this_thread::get_id();
    }

private:
    struct FrameBuffers
    {
        Diligent::RefCntAutoPtr<Diligent::IBuffer> vb;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> ib;
        std::uint32_t vbSize = 0;
        std::uint32_t ibSize = 0;
    };

    struct ResolvedTexture
    {
        Graphics::TextureHandle handle{};
        Diligent::ITextureView* srv = nullptr;
    };

    [[nodiscard]] ResolvedTexture ResolveTexture(
        RenderOverlayTextureHandle texture) noexcept;
    [[nodiscard]] bool CheckRenderThread(const char* operation) const noexcept;

    DiligentDeviceServices m_services{};
    DiligentNativeResourceOwner* m_nativeResources = nullptr;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_srb;
    Diligent::IShaderResourceVariable* m_textureVariable = nullptr;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_cb;
    std::vector<FrameBuffers> m_frameBuffers;
    DiligentOverlayTextureRegistry m_textureRegistry;
    std::thread::id m_ownerThread{};
    bool m_ready = false;
};

} // namespace SagaEngine::Render::Backend
