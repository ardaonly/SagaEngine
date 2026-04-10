/// @file Material.h
/// @brief Data-only material description shared by renderer and asset pipeline.
///
/// Layer  : SagaEngine / Render / Materials
/// Purpose: A material is the union of a shader template, a parameter
///          block (colour, roughness, tint, UV scroll), and a small set
///          of texture bindings (albedo, normal, MRA, emissive).  The
///          renderer wants to read this in a tight inner loop; the
///          asset pipeline wants to write it from a JSON/YAML file; the
///          editor wants to validate and visualise it; the streaming
///          manager wants to know which texture references are still
///          pending.  All four callers ship in different translation
///          units with different dependency trees, so the material
///          description itself has to be a strict data-only header:
///          no GPU handles, no backend types, no <glm/*>.
///
///          This file is that data-only description.  `MaterialAsset`
///          is the on-disk form (text string ids for everything
///          resolvable later); `MaterialRuntime` is the post-resolve
///          form the renderer consumes (interned shader template
///          handle, resolved texture ids).  The renderer's shader
///          binding layer lives elsewhere — this header only defines
///          what a material *is*, not how it reaches the GPU.
///
/// Design rules:
///   - Public header MUST stay GLM-free.  Vec3/Vec4 come from
///     `Math/Vec3.h` and `Math/Vec4.h`, same as every other public
///     header in the engine.
///   - Parameter storage is a small inline array.  Materials with
///     more than `kMaxScalarParams` or `kMaxVectorParams` are a code
///     smell — usually a shader author trying to pack a renderer
///     feature into a material block.  Static caps are visible to
///     the editor tools so they can warn authors earlier.
///   - Textures reference the asset database by stable id, not by
///     path.  Paths change across releases and pin us to the content
///     tree layout; stable ids survive rename and folder shuffles.
///
/// What this header is NOT:
///   - Not a shader.  A material *points at* a shader template by
///     name; the template itself is compiled upstream.
///   - Not a material instance.  We flatten that distinction at runtime
///     because MMO materials rarely need deep inheritance — the few
///     that do are baked during content build.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Render {

// ─── Local 4-component vector ──────────────────────────────────────────────

/// Compact 4-tuple used for material colour / parameter storage.
/// Defined locally instead of pulling in a general-purpose `Vec4` type
/// because the material header must stay ultra-lightweight — the
/// renderer inner loop copies millions of these per frame and a
/// dependency on the full math library would drag its includes into
/// every translation unit that touches a draw call.  Semantics are
/// "just four floats"; no operator overloads, no constexpr math.
struct MaterialVec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

// ─── Limits ─────────────────────────────────────────────────────────────────

/// Hard caps on the material parameter block.  Picked so the combined
/// footprint of a single `MaterialRuntime` stays under one cache line
/// per parameter group — the renderer iterates millions of these per
/// frame on a busy scene and cache pressure is the bottleneck.
inline constexpr std::size_t kMaxScalarParams  = 8;
inline constexpr std::size_t kMaxVectorParams  = 8;
inline constexpr std::size_t kMaxTextureSlots  = 8;

/// Texture slot semantic.  A material uses the *index* (0..kMaxTextureSlots-1)
/// to look up the slot; this enum exists so asset tools, docs, and logs
/// can refer to the well-known slot names consistently.
enum class MaterialTextureSlot : std::uint8_t
{
    Albedo    = 0,
    Normal    = 1,
    MRA       = 2, ///< Metallic / Roughness / Ambient Occlusion packed.
    Emissive  = 3,
    Detail    = 4,
    Mask      = 5,
    Custom0   = 6,
    Custom1   = 7,
};

/// Render queue bucket.  Controls draw ordering and alpha handling.
/// Opaque and AlphaTest can write depth; Transparent draws after
/// opaque, back-to-front sorted.  Overlay is for HUD / nameplate
/// style passes that bypass depth entirely.
enum class MaterialRenderQueue : std::uint8_t
{
    Opaque      = 0,
    AlphaTest   = 1,
    Transparent = 2,
    Overlay     = 3,
};

/// Culling mode.  Exposed at the material level (not the shader level)
/// so a single shader template can serve both double-sided foliage and
/// single-sided geometry by flipping this flag in the material asset.
enum class MaterialCullMode : std::uint8_t
{
    Back  = 0, ///< Default — hide back-facing triangles.
    Front = 1, ///< For inside-out effects like skybox.
    None  = 2, ///< Double-sided — foliage, cloth, decals.
};

// ─── Parameter blocks ───────────────────────────────────────────────────────

/// Named scalar parameter in the on-disk asset.  The name is matched
/// against the shader template's parameter block at resolve time; a
/// name that the shader does not expose is dropped with a warning so
/// the artist sees feedback but the build does not fail.
struct MaterialScalarParam
{
    std::string name;
    float       value = 0.0f;
};

struct MaterialVectorParam
{
    std::string             name;
    MaterialVec4  value{};
};

/// Asset-side texture reference.  The `assetId` is the stable content
/// id; `fallbackPath` exists so content authors can still hand-edit
/// during bring-up.  The pipeline resolves `assetId` first and only
/// consults `fallbackPath` on debug builds.
struct MaterialTextureRef
{
    MaterialTextureSlot slot     = MaterialTextureSlot::Albedo;
    std::uint64_t       assetId  = 0;
    std::string         fallbackPath;
};

// ─── On-disk asset form ─────────────────────────────────────────────────────

/// The shape the asset importer produces and the editor round-trips.
/// Everything is string/id-addressable because we are still upstream
/// of the resolve step — the shader template is a name, the textures
/// are asset ids, the parameters are name/value pairs.
struct MaterialAsset
{
    std::uint64_t materialId = 0;
    std::string   debugName;
    std::string   shaderTemplateName;

    MaterialRenderQueue renderQueue = MaterialRenderQueue::Opaque;
    MaterialCullMode    cullMode    = MaterialCullMode::Back;
    bool                writesDepth = true;
    bool                receivesShadows = true;
    bool                castsShadows    = true;
    float               alphaTestThreshold = 0.5f;

    std::vector<MaterialScalarParam> scalarParams;
    std::vector<MaterialVectorParam> vectorParams;
    std::vector<MaterialTextureRef>  textures;
};

// ─── Runtime form ───────────────────────────────────────────────────────────

/// Opaque handle returned by the shader template registry.  Stays
/// plain POD so the material table can be memcpy-moved.
enum class ShaderTemplateHandle : std::uint32_t { kInvalid = 0xFFFFFFFFu };

/// Handle into the texture streaming table.  A negative / sentinel
/// value means "placeholder texture, stream not ready yet".
enum class TextureHandle : std::uint32_t { kInvalid = 0xFFFFFFFFu };

/// Post-resolve material.  The renderer hot path reads this — every
/// field is fixed-size, POD-friendly, and cache-line-conscious.
struct MaterialRuntime
{
    std::uint64_t         materialId   = 0;
    ShaderTemplateHandle  shaderHandle = ShaderTemplateHandle::kInvalid;
    MaterialRenderQueue   renderQueue  = MaterialRenderQueue::Opaque;
    MaterialCullMode      cullMode     = MaterialCullMode::Back;
    bool                  writesDepth      = true;
    bool                  receivesShadows  = true;
    bool                  castsShadows     = true;
    float                 alphaTestThreshold = 0.5f;

    /// Flat parameter arrays, indexed 1:1 with the shader template's
    /// parameter block.  Unused slots are zero.  Keeping everything
    /// in a fixed array (vs. a hash map) makes the draw submission
    /// path purely copy-then-bind; the indirect lookup the map would
    /// force us into is visible in profilers at this call rate.
    std::array<float,                  kMaxScalarParams> scalars{};
    std::array<MaterialVec4, kMaxVectorParams> vectors{};
    std::array<TextureHandle,          kMaxTextureSlots> textures{
        TextureHandle::kInvalid, TextureHandle::kInvalid, TextureHandle::kInvalid,
        TextureHandle::kInvalid, TextureHandle::kInvalid, TextureHandle::kInvalid,
        TextureHandle::kInvalid, TextureHandle::kInvalid,
    };
};

// ─── Defaults ───────────────────────────────────────────────────────────────

/// Builds a default opaque material.  Used as a fallback when a mesh
/// references a material id that failed to resolve — a missing pink
/// "error material" is immediately visible to artists without crashing
/// the renderer.
[[nodiscard]] inline MaterialRuntime MakeErrorMaterial() noexcept
{
    MaterialRuntime mat;
    mat.materialId = 0;
    mat.shaderHandle = ShaderTemplateHandle::kInvalid;
    mat.renderQueue  = MaterialRenderQueue::Opaque;
    mat.cullMode     = MaterialCullMode::Back;
    // Neon pink in the first vector param — the error shader template
    // should sample `vectors[0]` as the base colour.  Hard-coded here
    // so the visual is consistent across every subsystem that loads
    // a missing material.
    mat.vectors[0] = { 1.0f, 0.0f, 1.0f, 1.0f };
    return mat;
}

} // namespace SagaEngine::Render
