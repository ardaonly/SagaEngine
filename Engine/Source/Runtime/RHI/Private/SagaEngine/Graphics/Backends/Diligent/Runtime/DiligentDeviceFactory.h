/// @file DiligentDeviceFactory.h
/// @brief Runtime-owned Diligent device and swapchain factory helpers.

#pragma once

#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

#include "RefCntAutoPtr.hpp"

namespace Diligent
{
struct IDeviceContext;
struct IRenderDevice;
struct ISwapChain;
} // namespace Diligent

namespace SagaEngine::Render::Backend
{

[[nodiscard]] GraphicsBackendAPI TryInitAPI(
    GraphicsBackendAPI preferred,
    const SwapchainDesc& desc,
    const RenderBackendConfig& cfg,
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap);

} // namespace SagaEngine::Render::Backend
