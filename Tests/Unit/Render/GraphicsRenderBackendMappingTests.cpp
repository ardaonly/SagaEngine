/// @file GraphicsRenderBackendMappingTests.cpp
/// @brief Tests private SagaGraphics to Render backend DTO mapping.

#include "SagaEngine/Graphics/Backend/GraphicsRenderBackendMapping.h"

#include <gtest/gtest.h>

namespace
{

namespace Graphics = SagaEngine::Graphics;
namespace Mapping = SagaEngine::Graphics::Backend;
namespace RenderBackend = SagaEngine::Render::Backend;

TEST(GraphicsRenderBackendMapping, MapsBackendPreferencesToRenderApis)
{
    EXPECT_EQ(
        Mapping::ToRenderBackendAPI(Graphics::BackendPreference::Auto),
        RenderBackend::GraphicsBackendAPI::kAuto);
    EXPECT_EQ(
        Mapping::ToRenderBackendAPI(Graphics::BackendPreference::NativePrimary),
        RenderBackend::GraphicsBackendAPI::kNativePrimary);
    EXPECT_EQ(
        Mapping::ToRenderBackendAPI(Graphics::BackendPreference::NativePortable),
        RenderBackend::GraphicsBackendAPI::kNativePortable);
    EXPECT_EQ(
        Mapping::ToRenderBackendAPI(Graphics::BackendPreference::NativeLegacy),
        RenderBackend::GraphicsBackendAPI::kNativeLegacy);
    EXPECT_EQ(
        Mapping::ToRenderBackendAPI(Graphics::BackendPreference::Compatibility),
        RenderBackend::GraphicsBackendAPI::kCompatibility);
}

TEST(GraphicsRenderBackendMapping, MapsRenderApisToBackendPreferences)
{
    EXPECT_EQ(
        Mapping::ToGraphicsBackendPreference(RenderBackend::GraphicsBackendAPI::kAuto),
        Graphics::BackendPreference::Auto);
    EXPECT_EQ(
        Mapping::ToGraphicsBackendPreference(RenderBackend::GraphicsBackendAPI::kNativePrimary),
        Graphics::BackendPreference::NativePrimary);
    EXPECT_EQ(
        Mapping::ToGraphicsBackendPreference(RenderBackend::GraphicsBackendAPI::kNativePortable),
        Graphics::BackendPreference::NativePortable);
    EXPECT_EQ(
        Mapping::ToGraphicsBackendPreference(RenderBackend::GraphicsBackendAPI::kNativeLegacy),
        Graphics::BackendPreference::NativeLegacy);
    EXPECT_EQ(
        Mapping::ToGraphicsBackendPreference(RenderBackend::GraphicsBackendAPI::kCompatibility),
        Graphics::BackendPreference::Compatibility);
}

TEST(GraphicsRenderBackendMapping, UnknownValuesMapToAuto)
{
    EXPECT_EQ(
        Mapping::ToRenderBackendAPI(static_cast<Graphics::BackendPreference>(255)),
        RenderBackend::GraphicsBackendAPI::kAuto);
    EXPECT_EQ(
        Mapping::ToGraphicsBackendPreference(
            static_cast<RenderBackend::GraphicsBackendAPI>(255)),
        Graphics::BackendPreference::Auto);
}

TEST(GraphicsRenderBackendMapping, MapsBackendConfig)
{
    Graphics::RenderBackendDesc desc{};
    desc.preferredBackend = Graphics::BackendPreference::NativePortable;
    desc.enableValidation = true;
    desc.headless = true;

    const auto config = Mapping::ToRenderBackendConfig(desc);
    EXPECT_EQ(
        config.preferredAPI,
        RenderBackend::GraphicsBackendAPI::kNativePortable);
    EXPECT_TRUE(config.enableValidation);
}

TEST(GraphicsRenderBackendMapping, MapsBackendStatus)
{
    RenderBackend::RenderBackendStatus renderStatus{};
    renderStatus.selectedAPI = RenderBackend::GraphicsBackendAPI::kCompatibility;
    renderStatus.frameIndex = 42u;
    renderStatus.initialized = true;

    const auto graphicsStatus =
        Mapping::ToGraphicsBackendStatus(renderStatus);

    EXPECT_EQ(
        graphicsStatus.selectedBackend,
        Graphics::BackendPreference::Compatibility);
    EXPECT_EQ(graphicsStatus.frameIndex, 42u);
    EXPECT_TRUE(graphicsStatus.initialized);
    EXPECT_EQ(
        graphicsStatus.health,
        Graphics::RenderBackendHealth::Ready);
    EXPECT_EQ(
        graphicsStatus.failure,
        Graphics::RenderBackendFailure::None);
}

TEST(GraphicsRenderBackendMapping, ClampsQualityPresetToCapabilityCeiling)
{
    Graphics::RenderBackendCapabilities capabilities{};
    capabilities.qualityCeiling = Graphics::RenderQualityPreset::Medium;

    EXPECT_EQ(
        Graphics::ClampRenderQualityPreset(
            Graphics::RenderQualityPreset::Low,
            capabilities),
        Graphics::RenderQualityPreset::Low);
    EXPECT_EQ(
        Graphics::ClampRenderQualityPreset(
            Graphics::RenderQualityPreset::Ultra,
            capabilities),
        Graphics::RenderQualityPreset::Medium);
}

TEST(GraphicsRenderBackendMapping, ResolvesUnsupportedFeatureFallback)
{
    EXPECT_EQ(
        Graphics::ResolveRenderCapabilityFallback(
            Graphics::RenderFeatureSupport::Unsupported,
            false),
        Graphics::RenderCapabilityFallback::NotRequested);
    EXPECT_EQ(
        Graphics::ResolveRenderCapabilityFallback(
            Graphics::RenderFeatureSupport::Supported,
            true),
        Graphics::RenderCapabilityFallback::Available);
    EXPECT_EQ(
        Graphics::ResolveRenderCapabilityFallback(
            Graphics::RenderFeatureSupport::Unsupported,
            true),
        Graphics::RenderCapabilityFallback::DisabledUnsupported);
}

} // namespace
