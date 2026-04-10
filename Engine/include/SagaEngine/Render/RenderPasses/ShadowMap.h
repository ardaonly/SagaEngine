/// @file ShadowMap.h
/// @brief Shadow map allocation, cascade split, and pass descriptor.
///
/// Layer  : SagaEngine / Render / RenderPasses
/// Purpose: Dynamic shadows are the most expensive part of a
///          real-time renderer — a single light can easily double
///          the frame cost if its shadow pass is not carefully
///          budgeted.  This header defines the data structures that
///          describe shadow map *allocation* (what resolution,
///          which atlas slot), the *cascade split* strategy used
///          for directional lights, and the *pass descriptor* the
///          render graph consumes to schedule a shadow render.  It
///          is deliberately declarative: the render graph and the
///          RHI do the work, but the shape of the work is stated
///          in types that are reviewable in isolation.
///
/// Design rules:
///   - Shadow maps live in an atlas.  A per-light texture would
///     trash the GPU bind cache and scale poorly past a dozen
///     lights; an atlas folds every shadow into one big texture
///     and uses a subrect per light.  The atlas is a fixed size
///     chosen at boot, not a runtime growable resource.
///   - Cascade splits for directional lights use the "practical
///     split scheme" (a weighted blend of linear and logarithmic
///     splits).  The weight is exposed here as `cascadeLambda`
///     instead of being hardcoded — different content targets
///     prefer different blends.
///   - Shadow bias is stored per cascade, not per light.  Farther
///     cascades have bigger texel sizes and benefit from more
///     aggressive bias; applying the same bias to all cascades
///     causes either acne on the near cascades or Peter Panning
///     on the far cascades.
///   - Point-light cubemap shadows are represented as six entries
///     in the atlas plus a small cube-face table.  A dedicated
///     cubemap render target would bloat GPU memory at no
///     quality win for the low to mid lights that dominate MMO
///     scenes.
///
/// What this header is NOT:
///   - Not a PCF sampling filter.  The filter kernel lives in the
///     shader; this header only describes the source data.
///   - Not a light culling pass.  Culling happens upstream; by the
///     time a shadow pass descriptor is built, the light list has
///     already been trimmed to casters that matter.

#pragma once

#include "SagaEngine/Render/Materials/Material.h" // MaterialVec4 (local)

#include <array>
#include <cstdint>
#include <vector>

namespace SagaEngine::Render {

// ─── Geometry helpers ──────────────────────────────────────────────────────

/// Axis-aligned atlas rectangle, in texels.  The shadow pass reads
/// `MaterialVec4`-style storage for positions and matrices; bringing
/// in the full math library for a UV rect is overkill.
struct ShadowAtlasRect
{
    std::uint32_t x      = 0;
    std::uint32_t y      = 0;
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
};

/// 4x4 column-major matrix as plain floats.  Used for the shadow
/// view / projection that the shader multiplies into world space.
/// Declared locally to keep this header independent of `Mat4.h` —
/// the renderer can populate it from a `Mat4` via a simple memcpy.
struct ShadowMatrix4
{
    std::array<float, 16> m{};
};

// ─── Light classes ─────────────────────────────────────────────────────────

enum class ShadowLightKind : std::uint8_t
{
    Directional = 0,
    Spot        = 1,
    Point       = 2,
};

/// Quality tier.  Affects both the atlas budget assigned to the
/// light and the number of PCF taps used by the sampling shader.
/// Expressed as a coarse enum so the editor and the cvar system can
/// talk about the same thing without negotiating a float slider.
enum class ShadowQuality : std::uint8_t
{
    Off    = 0,
    Low    = 1,
    Medium = 2,
    High   = 3,
    Ultra  = 4,
};

// ─── Cascade split configuration ───────────────────────────────────────────

/// Configuration for a directional light's cascaded shadow map.
/// Used by the shadow pass to compute the per-cascade view and
/// projection; the result is written into the per-cascade
/// `ShadowMatrix4` that the shader consumes.
struct DirectionalCascadeConfig
{
    /// Number of cascades.  Four is the default — three is too
    /// coarse for the typical open-world scene and five rarely
    /// earns its memory cost.  Values above `kMaxDirectionalCascades`
    /// are clamped.
    std::uint8_t cascadeCount = 4;

    /// Practical split weight.  0.0 = purely linear splits (good
    /// for flat scenes, wastes texels near the camera); 1.0 = purely
    /// logarithmic (sharp near, muddy far).  0.6 is the typical
    /// MMO starting point.
    float cascadeLambda = 0.6f;

    /// Far plane used as the outer bound of the last cascade.  Set
    /// to the game camera's max view distance, not an arbitrary
    /// large number — oversized cascades waste texels on empty
    /// space.
    float maxShadowDistance = 200.0f;

    /// Per-cascade depth bias.  Index i applies to cascade i.  The
    /// renderer skips unused entries based on `cascadeCount`.
    std::array<float, 4> depthBias     = { 0.0005f, 0.0010f, 0.0025f, 0.0050f };
    std::array<float, 4> slopeScaleBias = { 1.0f, 1.5f, 2.5f, 4.0f };
};

// ─── Per-light pass descriptor ─────────────────────────────────────────────

/// Describes one light's shadow pass.  The render graph consumes an
/// array of these to build the scheduling graph; the values are
/// filled in by the shadow culling pass upstream.
struct ShadowPassDescriptor
{
    ShadowLightKind kind         = ShadowLightKind::Directional;
    ShadowQuality   quality      = ShadowQuality::Medium;
    std::uint32_t   lightId      = 0;

    /// Atlas slots assigned to this light.  Directional lights use
    /// up to four slots (one per cascade); spot lights use one;
    /// point lights use six (one per cube face).
    std::array<ShadowAtlasRect, 6> atlasSlots{};
    std::uint8_t                   atlasSlotCount = 0;

    /// View / projection matrices for each slot.  Index i pairs
    /// with `atlasSlots[i]`.
    std::array<ShadowMatrix4, 6> viewProjections{};

    /// Per-slot bias (reuses the directional config style for
    /// spot and point lights, which makes the sampling shader
    /// branch-free).
    std::array<float, 6> depthBias      = { 0, 0, 0, 0, 0, 0 };
    std::array<float, 6> slopeScaleBias = { 0, 0, 0, 0, 0, 0 };

    /// True when the light's shape or transform has changed since
    /// the last frame.  Static lights are skipped by the shadow
    /// pass if this flag is false and their slots are already
    /// resident in the atlas.
    bool needsRedraw = true;
};

// ─── Atlas allocation policy ───────────────────────────────────────────────

/// Quality → texel budget table.  The shadow system uses this to
/// decide how many atlas texels each quality tier is allowed to
/// consume.  Expressed as a square side length; the atlas allocator
/// rounds up to a power of two before carving a slot.
struct ShadowQualityBudget
{
    std::array<std::uint32_t, 5> sideLengthForQuality = {
        0,    ///< Off       — no allocation.
        256,  ///< Low
        512,  ///< Medium
        1024, ///< High
        2048, ///< Ultra
    };
};

/// Full atlas configuration.  One instance lives on the renderer;
/// content scripts can tweak it at boot to trade off quality for
/// memory.
struct ShadowAtlasConfig
{
    /// Atlas size in texels.  Power of two; common values are
    /// 4096 (budget), 8192 (default), 16384 (high-end).
    std::uint32_t atlasSize = 8192;

    /// Depth format id understood by the RHI.  Opaque here — the
    /// renderer maps it to the backend's enum.
    std::uint32_t depthFormat = 0;

    ShadowQualityBudget budget{};

    /// When true, the allocator reshuffles the atlas every frame to
    /// defragment.  Off by default because the shuffle cost is
    /// higher than the defrag benefit in the typical MMO scene.
    bool repackEveryFrame = false;
};

// ─── Limits ────────────────────────────────────────────────────────────────

/// Hard cap on the number of directional cascades.  More than this
/// bloats the shadow matrix UBO and yields diminishing returns.
inline constexpr std::uint8_t kMaxDirectionalCascades = 4;

/// Maximum shadow-casting lights per frame.  The shadow pass refuses
/// to process more than this; the culling pass upstream is expected
/// to trim the list to the highest-impact casters.
inline constexpr std::uint32_t kMaxShadowCastersPerFrame = 64;

} // namespace SagaEngine::Render
