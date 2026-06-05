/// @file RenderPipelineConfig.h
/// @brief Transitional public render pipeline configuration DTOs.

#pragma once

#include <cstdint>

namespace SagaEngine::Render
{

enum class RenderPipelineProfile : std::uint8_t
{
    Default = 0,
    Lightweight,
    Forward,
    Deferred,
    Custom,
};

enum class RenderPipelineQualityLevel : std::uint8_t
{
    Low = 0,
    Medium,
    High,
    Ultra,
    Custom,
};

enum class RenderPipelineStatusCode : std::uint8_t
{
    Unknown = 0,
    Ready,
    Disabled,
    Unsupported,
    InvalidConfig,
};

enum class RenderPostProcessQuality : std::uint8_t
{
    Low = 0,
    Medium,
    High,
    Ultra,
    Custom,
};

enum class RenderPostProcessOutputFormat : std::uint8_t
{
    Backbuffer = 0,
    StandardDynamicRange,
    HighDynamicRange,
    HalfFloat,
    Custom,
};

enum class RenderAntiAliasingMode : std::uint8_t
{
    None = 0,
    FastApproximate,
    Temporal,
    Custom,
};

enum class RenderToneMapOperator : std::uint8_t
{
    None = 0,
    Reinhard,
    Filmic,
    Aces,
    AgX,
    Custom,
};

enum class RenderExposureMode : std::uint8_t
{
    Manual = 0,
    Automatic,
    Custom,
};

enum class RenderColorGradingStyle : std::uint8_t
{
    None = 0,
    Neutral,
    Filmic,
    LookupTable,
    Custom,
};

enum class RenderShadowQuality : std::uint8_t
{
    Off = 0,
    Low,
    Medium,
    High,
    Ultra,
    Custom,
};

struct RenderFeatureSet
{
    bool shadows = true;
    bool postProcessing = true;
    bool highDynamicRange = true;
    bool bloom = false;
    bool antiAliasing = false;
    bool temporalUpscaling = false;
    bool wireframe = false;
    bool debugVisualization = false;
};

struct RenderPipelineQualitySettings
{
    RenderPipelineQualityLevel quality = RenderPipelineQualityLevel::High;
    float renderScale = 1.0f;
    float lodBias = 1.0f;
    std::uint32_t maxDrawItemsPerView = 8192;
    std::uint32_t shadowMapResolution = 2048;
    std::uint32_t antiAliasingSamples = 1;
};

struct RenderPipelineDebugOptions
{
    bool showCulling = false;
    bool showLighting = false;
    bool showOverdraw = false;
    bool captureStats = false;
    bool deterministicOrdering = true;
};

struct RenderTemporalAntiAliasingConfig
{
    bool enabled = false;
    RenderPostProcessQuality quality = RenderPostProcessQuality::Medium;
    float historyWeight = 0.9f;
    float jitterScale = 1.0f;
};

struct RenderBloomConfig
{
    bool enabled = false;
    RenderPostProcessQuality quality = RenderPostProcessQuality::Medium;
    float threshold = 1.0f;
    float softKnee = 0.5f;
    float intensity = 0.05f;
    std::uint32_t mipCount = 6;
};

struct RenderToneMapConfig
{
    bool enabled = true;
    RenderToneMapOperator op = RenderToneMapOperator::Aces;
    float exposureCompensationEV = 0.0f;
    float whitePoint = 11.2f;
};

struct RenderExposureConfig
{
    bool enabled = true;
    RenderExposureMode mode = RenderExposureMode::Automatic;
    float minEv100 = -3.0f;
    float maxEv100 = 16.0f;
    float adaptationSpeed = 1.1f;
    float meterKey = 0.18f;
};

struct RenderColorGradingConfig
{
    bool enabled = false;
    RenderColorGradingStyle style = RenderColorGradingStyle::Neutral;
    float intensity = 1.0f;
    float saturation = 1.0f;
    float contrast = 1.0f;
};

struct RenderPostProcessConfig
{
    bool enabled = true;
    bool depthOfField = false;
    bool motionBlur = false;
    bool sharpening = false;
    bool vignette = false;
    bool filmGrain = false;
    bool chromaticAberration = false;
    RenderPostProcessQuality quality = RenderPostProcessQuality::High;
    RenderPostProcessOutputFormat outputFormat =
        RenderPostProcessOutputFormat::Backbuffer;
    RenderAntiAliasingMode antiAliasing = RenderAntiAliasingMode::None;
    RenderTemporalAntiAliasingConfig temporalAntiAliasing{};
    RenderBloomConfig bloom{};
    RenderToneMapConfig toneMap{};
    RenderExposureConfig exposure{};
    RenderColorGradingConfig colorGrading{};
};

struct RenderShadowBiasConfig
{
    float depthBias0 = 0.0005f;
    float depthBias1 = 0.0010f;
    float depthBias2 = 0.0025f;
    float depthBias3 = 0.0050f;
    float slopeScaleBias0 = 1.0f;
    float slopeScaleBias1 = 1.5f;
    float slopeScaleBias2 = 2.5f;
    float slopeScaleBias3 = 4.0f;
};

struct RenderShadowConfig
{
    bool enabled = true;
    RenderShadowQuality quality = RenderShadowQuality::High;
    std::uint32_t atlasSize = 8192;
    std::uint32_t maxCasters = 64;
    std::uint32_t cascadeCount = 4;
    float cascadeLambda = 0.6f;
    float maxDistance = 200.0f;
    RenderShadowBiasConfig bias{};
};

struct RenderPipelineConfig
{
    RenderPipelineProfile profile = RenderPipelineProfile::Default;
    RenderFeatureSet features{};
    RenderPipelineQualitySettings quality{};
    RenderPostProcessConfig postProcess{};
    RenderShadowConfig shadows{};
    RenderPipelineDebugOptions debug{};
    std::uint32_t maxViews = 1;
    bool enabled = true;
};

struct RenderPipelineStatus
{
    RenderPipelineStatusCode status = RenderPipelineStatusCode::Unknown;
    bool accepted = false;
    bool active = false;
    std::uint32_t warningCount = 0;
    std::uint32_t unsupportedFeatureCount = 0;
};

struct RenderPipelineStats
{
    std::uint64_t framesSubmitted = 0;
    std::uint32_t activeViews = 0;
    std::uint32_t drawItemCount = 0;
    std::uint32_t culledItemCount = 0;
    float cpuFrameMilliseconds = 0.0f;
    float gpuFrameMilliseconds = 0.0f;
};

} // namespace SagaEngine::Render
