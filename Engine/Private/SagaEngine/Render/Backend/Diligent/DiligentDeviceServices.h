/// @file DiligentDeviceServices.h
/// @brief Private borrowed Diligent device/context/swapchain service view.

#pragma once

namespace Diligent
{
struct IRenderDevice;
struct IDeviceContext;
struct ISwapChain;
} // namespace Diligent

namespace SagaEngine::Render::Backend
{

struct DiligentDeviceServices
{
    [[nodiscard]] Diligent::IRenderDevice* Device() const noexcept
    {
        return device;
    }

    [[nodiscard]] Diligent::IDeviceContext* ImmediateContext() const noexcept
    {
        return immediateContext;
    }

    [[nodiscard]] Diligent::ISwapChain* SwapChain() const noexcept
    {
        return swapChain;
    }

    [[nodiscard]] bool IsBound() const noexcept
    {
        return device != nullptr && immediateContext != nullptr &&
               swapChain != nullptr;
    }

    Diligent::IRenderDevice* device = nullptr;
    Diligent::IDeviceContext* immediateContext = nullptr;
    Diligent::ISwapChain* swapChain = nullptr;
};

} // namespace SagaEngine::Render::Backend
