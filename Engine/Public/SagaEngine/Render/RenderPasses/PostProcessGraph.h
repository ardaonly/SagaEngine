/// @file PostProcessGraph.h
/// @brief Declarative post-processing chain description consumed by the render graph.
///
/// Layer  : SagaEngine / Render / RenderPasses
/// Purpose: After the lighting pass finishes writing the HDR scene
///          color target, a chain of screen-space effects runs to
///          turn that raw radiance into the final image the backbuffer
///          shows.  Tone mapping, bloom, temporal anti-aliasing,
///          color grading, sharpening, vignette, film grain — each is
///          its own pass with its own inputs and parameters, and the
///          order they run in matters: bloom has to see pre-tonemapped
///          HDR, TAA has to see the jittered pre-grading image, film
///          grain has to be the last thing that touches the pixels so
///          it is not smeared by a subsequent blur.
///
///          This header defines the *declarative description* of such
///          a chain.  A `PostProcessGraph` is a data structure the
///          editor and cvar system can edit, serialise, and hand to
///          the render graph without ever calling into a backend API.
///          The render graph decides how to schedule the stages, which
///          targets to reuse, and which effects to skip when they are
///          disabled; this header only states *what* the chain looks
///          like.
///
/// Design rules:
///   - Effects are identified by a stable enum, not by string.  The
///     editor surfaces a nice name and a tooltip; the runtime only
///     ever compares integers, because a post-process chain is
///     walked every frame and string compares would add measurable
///     CPU cost.
///   - Stage parameters are packed into a union-free `PostProcessStage`
///     struct with one parameter block per effect family.  A single
///     flat struct per stage beats a virtual hierarchy in every way
///     that matters here: the struct is trivially copyable, fits in
///     two cache lines, and the render graph can stamp a bunch of
///     them into a scratch buffer without running constructors.
///   - Order is explicit.  The chain is an `std::array` of slots, and
///     disabled stages leave their slots present but flagged off so
///     enabling an effect does not shift later stages into different
///     temporaries.  That keeps frame-to-frame render graph caching
///     stable.
///   - The output format is stated at chain level, not at stage level.
///     Intermediate stages may ping-pong through a couple of HDR
///     targets; the final write lands in whatever the chain declares
///     as the swapchain format.
///
/// What this header is NOT:
///   - Not the shaders.  The shader code lives in
///     `Render/Materials/Shaders` and is keyed off the same effect
///     enum so a disabled stage does not even compile into the frame.
///   - Not a screen-space reflection or SSGI pass.  Those belong in
///     the lighting stage of the render graph, before post-processing
///     runs on the resolved color.
///   - Not the debug overlay.  The overlay is a separate render-graph
///     leaf that composites on top of the chain's final output.

#pragma once

#include "SagaEngine/Render/Materials/Material.h" // MaterialVec4 (reused for packed parameters)

#include <array>
#include <cstdint>

namespace SagaEngine::Render {

// ─── Stage identification ──────────────────────────────────────────────────

/// Fixed set of post-process stage kinds.  New stages land here at
/// the end of the list; stable integer values are part of the
/// serialised asset so reordering breaks saved profiles.
///
/// The order in this enum is NOT the execution order.  Execution
/// order is stated by the array index inside `PostProcessGraph`; this
/// enum is just "which kind of effect is this slot".
enum class PostProcessStageKind : std::uint8_t
{
    None              = 0,
    TemporalAA        = 1,  ///< Runs first — needs the jittered raw color.
    AutoExposure      = 2,  ///< Computes EV100 from the luminance histogram.
    Bloom             = 3,  ///< Reads HDR; writes HDR.  Must precede tone map.
    DepthOfField      = 4,  ///< Physical circle-of-confusion DoF.
    MotionBlur        = 5,  ///< Per-object velocity buffer driven.
    ToneMap           = 6,  ///< HDR → LDR.  ACES / filmic / Reinhard.
    ColorGrading      = 7,  ///< 3D LUT lookup on the LDR output.
    Sharpen           = 8,  ///< CAS-style sharpening on the graded image.
    Fxaa              = 9,  ///< Cheap edge AA when TAA is disabled.
    Vignette          = 10, ///< Radial darkening near the screen edges.
    FilmGrain         = 11, ///< Temporal noise.  Always last.
    ChromaticAberration = 12,
};

/// Tone mapper operator choice.  Coarse enum so the cvar system can
/// name the option without knowing what any of them mean internally.
enum class ToneMapOperator : std::uint8_t
{
    None     = 0, ///< Passthrough.  Debug use only — HDR on an SDR target clips.
    Reinhard = 1,
    Filmic   = 2, ///< Uncharted 2 style.
    Aces     = 3, ///< ACES Filmic Tone Mapping approximation.
    AgX      = 4, ///< Modern filmic curve with better hue preservation.
};

/// Anti-aliasing resolve strategy.  TAA and FXAA are mutually
/// exclusive — the chain allows both stages to exist but enabling
/// both at once is a content bug and the editor surfaces a warning.
enum class TemporalAaQuality : std::uint8_t
{
    Off    = 0,
    Low    = 1, ///< No history rectification.
    Medium = 2, ///< Colour clipping + variance clamp.
    High   = 3, ///< Above plus dilated motion vectors and neighborhood clip.
};

// ─── Per-effect parameter blocks ───────────────────────────────────────────

/// Bloom descriptor.  The bloom pass builds a pyramid of downsamples
/// and composites them back weighted by `intensity`.  `threshold` is
/// a soft knee — pixels just below the threshold contribute partially,
/// which avoids the hard-edged "bloom popping" artefact common in
/// naive implementations.
struct BloomParams
{
    float threshold        = 1.0f;
    float softKnee         = 0.5f;
    float intensity        = 0.05f;
    std::uint8_t mipCount  = 6;  ///< Pyramid depth.  6 is the classic default.
    std::uint8_t _pad[3]   = { 0, 0, 0 };
};

/// Tone mapping descriptor.  `exposureCompensationEV` is applied on
/// top of the auto-exposure result; content authors use it as a
/// grading knob without having to fight the luminance histogram.
struct ToneMapParams
{
    ToneMapOperator op                     = ToneMapOperator::Aces;
    std::uint8_t    _pad[3]                = { 0, 0, 0 };
    float           exposureCompensationEV = 0.0f;
    float           whitePoint             = 11.2f; ///< Luminance that maps to 1.0; only used by Filmic.
};

/// Temporal AA descriptor.  `historyWeight` is the alpha of the
/// exponential history accumulator; higher values look smoother but
/// smear more on motion.  The clamp modes live in the shader — this
/// header only exposes the knobs a content author should touch.
struct TemporalAaParams
{
    TemporalAaQuality quality      = TemporalAaQuality::Medium;
    std::uint8_t      _pad[3]      = { 0, 0, 0 };
    float             historyWeight = 0.9f;
    float             jitterScale   = 1.0f; ///< Multiplier on the Halton jitter amplitude.
};

/// Auto exposure descriptor.  The luminance histogram lives in a
/// compute pass; these parameters drive its adaptation curve.
struct AutoExposureParams
{
    float minEv100        = -3.0f;
    float maxEv100        = 16.0f;
    float adaptationSpeed = 1.1f; ///< Exponential speed in EV/second.
    float meterKey        = 0.18f; ///< Middle-grey target luminance (18% grey).
};

/// Depth-of-field descriptor.  Physical model: the focal length and
/// aperture drive the circle-of-confusion that the blur kernel uses.
struct DepthOfFieldParams
{
    float focusDistanceMeters = 5.0f;
    float apertureFStop       = 4.0f;
    float focalLengthMm       = 50.0f;
    float maxBokehRadiusPx    = 16.0f; ///< Upper bound so distant content does not turn to soup.
};

/// Motion blur descriptor.  `sampleCount` trades quality for cost;
/// eight samples are the cheap default that covers 90% of the benefit
/// for a fraction of the cost of a full 32-sample gather.
struct MotionBlurParams
{
    std::uint8_t sampleCount     = 8;
    std::uint8_t _pad[3]         = { 0, 0, 0 };
    float        shutterAngleDeg = 180.0f; ///< 180° is the filmic default.
    float        maxBlurPixels   = 32.0f;
};

/// Colour grading descriptor.  Relies on a 3D LUT asset identified by
/// `lutAssetId`; the grading pass resolves the id through the asset
/// system so the chain description stays serialisation-friendly.
struct ColorGradingParams
{
    std::uint64_t lutAssetId   = 0;
    float         lutIntensity = 1.0f;
    float         saturation   = 1.0f;
    float         contrast     = 1.0f;
    float         _pad         = 0.0f;
};

/// Sharpening descriptor.  Intensity is the CAS-style knob; content
/// generally runs between 0.2 and 0.5.  Above 0.6 ringing becomes
/// objectionable.
struct SharpenParams
{
    float intensity     = 0.3f;
    float denoiseFactor = 0.0f; ///< Optional blur feedback to kill single-pixel noise.
};

/// Vignette descriptor.  Stored as a `MaterialVec4` so the shader can
/// fetch centre + falloff + roundness in one load.  Reuses the local
/// vector type declared in `Material.h` to avoid pulling the full
/// math library into the post-process header.
struct VignetteParams
{
    MaterialVec4 centreFalloffRoundness = { 0.5f, 0.5f, 0.6f, 1.0f };
    MaterialVec4 tintAndIntensity       = { 0.0f, 0.0f, 0.0f, 0.4f };
};

/// Film grain descriptor.  Temporal noise driven by a hashed
/// frame index; `responseCurve` selects whether the grain rides on
/// luminance or on a fixed amount.
struct FilmGrainParams
{
    float        intensity        = 0.03f;
    float        luminanceContrib = 0.5f; ///< 0 = flat grain, 1 = only in mid-tones.
    std::uint8_t responseCurve    = 1;    ///< 0 = linear, 1 = filmic, 2 = logarithmic.
    std::uint8_t _pad[3]          = { 0, 0, 0 };
};

/// Chromatic aberration descriptor.  Lens-imitation red/blue channel
/// offset, scaled by distance from the screen centre.
struct ChromaticAberrationParams
{
    float intensity  = 0.002f;
    float maxOffset  = 0.01f; ///< Upper bound so extreme settings do not decompose the image.
};

// ─── Stage slot ────────────────────────────────────────────────────────────

/// One slot in the chain.  Exactly one of the parameter blocks is
/// meaningful per slot, keyed by `kind`.  Rather than a `std::variant`
/// we store all blocks flat — the memory overhead is a few hundred
/// bytes per chain, which is nothing, and we get trivial copy /
/// memcpy into the render graph's scratch buffer.
struct PostProcessStage
{
    PostProcessStageKind kind    = PostProcessStageKind::None;
    bool                 enabled = false;
    std::uint8_t         _pad[2] = { 0, 0 };

    BloomParams               bloom{};
    ToneMapParams             toneMap{};
    TemporalAaParams          temporalAa{};
    AutoExposureParams        autoExposure{};
    DepthOfFieldParams        depthOfField{};
    MotionBlurParams          motionBlur{};
    ColorGradingParams        colorGrading{};
    SharpenParams             sharpen{};
    VignetteParams            vignette{};
    FilmGrainParams           filmGrain{};
    ChromaticAberrationParams chromaticAberration{};
};

// ─── Chain root ────────────────────────────────────────────────────────────

/// Fixed maximum number of post-process slots.  Twelve covers the
/// effects the engine ships; unused slots stay `kind = None, enabled
/// = false`.  The cap is part of the asset format so bumping it is a
/// content migration, not a silent behaviour change.
inline constexpr std::uint8_t kMaxPostProcessStages = 12;

/// Swapchain / output format selector.  Opaque integer mapped to the
/// RHI's real format enum inside the renderer — keeps this header
/// independent of any backend.
enum class PostProcessOutputFormat : std::uint8_t
{
    Rgba8Unorm      = 0, ///< Default SDR swapchain.
    Rgb10A2Unorm    = 1, ///< Higher-bit-depth SDR.
    Rgba16Float     = 2, ///< HDR swapchain for HDR10 output.
    Rgba16FloatSrgb = 3,
};

/// Root of the post-process chain.  Owned by the renderer, edited by
/// the content tools, and consumed by the render graph every frame.
struct PostProcessGraph
{
    std::array<PostProcessStage, kMaxPostProcessStages> stages{};

    /// Number of stages the chain should consider.  Stages beyond this
    /// index are skipped regardless of their `enabled` flag.
    std::uint8_t activeStageCount = 0;

    /// Final output format.  The last enabled stage writes into a
    /// target of this format.
    PostProcessOutputFormat outputFormat = PostProcessOutputFormat::Rgba8Unorm;

    /// When true, the render graph allocates its own history targets
    /// for TAA.  Off lets the caller supply existing targets — the
    /// stereoscopic VR path uses this to share history between eyes.
    bool ownsTemporalHistory = true;

    /// Flags a dirty chain so the render graph rebuilds its cached
    /// schedule.  Cleared by the renderer once the rebuild completes.
    bool dirty = true;

    std::uint8_t _pad[2] = { 0, 0 };
};

// ─── Construction helpers ──────────────────────────────────────────────────

/// Build the engine's default chain: TAA → AutoExposure → Bloom →
/// ToneMap → ColorGrading → Vignette → FilmGrain.  Used by the
/// content pipeline as the starting point for new profiles.
[[nodiscard]] inline PostProcessGraph MakeDefaultPostProcessGraph() noexcept
{
    PostProcessGraph g{};

    auto& taa         = g.stages[0];
    taa.kind          = PostProcessStageKind::TemporalAA;
    taa.enabled       = true;

    auto& exposure    = g.stages[1];
    exposure.kind     = PostProcessStageKind::AutoExposure;
    exposure.enabled  = true;

    auto& bloom       = g.stages[2];
    bloom.kind        = PostProcessStageKind::Bloom;
    bloom.enabled     = true;

    auto& toneMap     = g.stages[3];
    toneMap.kind      = PostProcessStageKind::ToneMap;
    toneMap.enabled   = true;

    auto& grading     = g.stages[4];
    grading.kind      = PostProcessStageKind::ColorGrading;
    grading.enabled   = false; ///< Off until content supplies a LUT.

    auto& vignette    = g.stages[5];
    vignette.kind     = PostProcessStageKind::Vignette;
    vignette.enabled  = true;

    auto& grain       = g.stages[6];
    grain.kind        = PostProcessStageKind::FilmGrain;
    grain.enabled     = true;

    g.activeStageCount = 7;
    g.outputFormat     = PostProcessOutputFormat::Rgba8Unorm;
    g.dirty            = true;
    return g;
}

} // namespace SagaEngine::Render
