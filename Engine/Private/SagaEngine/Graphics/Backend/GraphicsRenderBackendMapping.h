/// @file GraphicsRenderBackendMapping.h
/// @brief Private mapping between SagaGraphics shell DTOs and Render backend DTOs.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

namespace SagaEngine::Graphics::Backend
{

namespace RenderBackend = ::SagaEngine::Render::Backend;

[[nodiscard]] constexpr RenderBackend::GraphicsBackendAPI ToRenderBackendAPI(
    BackendPreference preference) noexcept
{
    switch (preference)
    {
        case BackendPreference::Auto:
            return RenderBackend::GraphicsBackendAPI::kAuto;
        case BackendPreference::NativePrimary:
            return RenderBackend::GraphicsBackendAPI::kNativePrimary;
        case BackendPreference::NativePortable:
            return RenderBackend::GraphicsBackendAPI::kNativePortable;
        case BackendPreference::NativeLegacy:
            return RenderBackend::GraphicsBackendAPI::kNativeLegacy;
        case BackendPreference::Compatibility:
            return RenderBackend::GraphicsBackendAPI::kCompatibility;
    }

    return RenderBackend::GraphicsBackendAPI::kAuto;
}

[[nodiscard]] constexpr BackendPreference ToGraphicsBackendPreference(
    RenderBackend::GraphicsBackendAPI api) noexcept
{
    switch (api)
    {
        case RenderBackend::GraphicsBackendAPI::kAuto:
            return BackendPreference::Auto;
        case RenderBackend::GraphicsBackendAPI::kNativePrimary:
            return BackendPreference::NativePrimary;
        case RenderBackend::GraphicsBackendAPI::kNativePortable:
            return BackendPreference::NativePortable;
        case RenderBackend::GraphicsBackendAPI::kNativeLegacy:
            return BackendPreference::NativeLegacy;
        case RenderBackend::GraphicsBackendAPI::kCompatibility:
            return BackendPreference::Compatibility;
    }

    return BackendPreference::Auto;
}

[[nodiscard]] constexpr RenderBackend::RenderBackendConfig ToRenderBackendConfig(
    const RenderBackendDesc& desc) noexcept
{
    RenderBackend::RenderBackendConfig config{};
    config.preferredAPI = ToRenderBackendAPI(desc.preferredBackend);
    config.enableValidation = desc.enableValidation;
    return config;
}

[[nodiscard]] constexpr RenderBackendStatus ToGraphicsBackendStatus(
    const RenderBackend::RenderBackendStatus& status) noexcept
{
    return {
        ToGraphicsBackendPreference(status.selectedAPI),
        status.frameIndex,
        status.initialized,
    };
}

} // namespace SagaEngine::Graphics::Backend
