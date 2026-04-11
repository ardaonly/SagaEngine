/// @file MeshAsset.h
/// @brief Data-only mesh description shared across renderer, physics, and streaming.
///
/// Layer  : SagaEngine / Render / Materials
/// Purpose: A mesh is the geometric companion to a material.  Several
///          subsystems need to look at the same mesh with very
///          different eyes: the renderer wants vertex buffers laid
///          out for fast draw submission, the physics system wants
///          a simplified convex hull, the streaming manager wants
///          per-LOD budgets, the asset importer wants a stable
///          description it can round-trip through a JSON file.
///          Pulling the renderer's runtime mesh type into all of
///          those call sites would make the build graph much
///          thornier than it needs to be.
///
///          This header defines the data-only mesh description.  As
///          with `Material.h`, the public surface is GLM-free; it
///          uses local compact vector types because the renderer
///          copies these descriptors in hot paths and the math
///          library's includes are overkill for "three floats".
///
/// Design rules:
///   - Vertices are stored as interleaved position / normal /
///     tangent / uv layouts.  Deinterleaving for specific passes is
///     the renderer's job at bind time, not the asset's job.
///   - Every mesh ships with at least one LOD.  LOD0 is the full-
///     detail mesh; higher LODs are optional.  The streaming
///     manager uses `vertexCountHint` and `indexCountHint` to
///     budget memory without decoding the whole LOD.
///   - Submeshes carry their own material id.  A single mesh can
///     reference several materials — typical for characters with a
///     body, a hair, and a cloth material.
///
/// What this header is NOT:
///   - Not a skeletal animation descriptor.  Bones and weights ride
///     a sibling header once skeletal is landed; this file is
///     strictly the static geometry.
///   - Not a GPU resource handle.  The runtime form lives under
///     `RHI` and keeps the GPU-side handles; this file is CPU-side
///     description only.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Render {

// ─── Local compact vector types ────────────────────────────────────────────

/// Three-component float tuple used for positions, normals, tangents,
/// and AABB corners.  Declared locally to keep this header free of
/// the full math library, mirroring the `MaterialVec4` decision in
/// `Material.h`.
struct MeshVec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct MeshVec2
{
    float u = 0.0f;
    float v = 0.0f;
};

// ─── Vertex layout ─────────────────────────────────────────────────────────

/// Interleaved vertex.  The renderer uploads the whole struct as-is
/// and the shader reads it through a fixed vertex layout.  We chose
/// interleaved because scattered streams hurt cache locality for the
/// typical forward renderer; deferred rendering benefits less but
/// still pays no penalty.  Size is 48 bytes, which is a comfortable
/// whole number of fetches on every GPU we target.
struct MeshVertex
{
    MeshVec3 position;
    MeshVec3 normal;
    MeshVec3 tangent;
    float    handedness = 1.0f; ///< Tangent basis handedness (+1 / -1).
    MeshVec2 uv0;
    MeshVec2 uv1;
};

static_assert(sizeof(MeshVertex) == 56,
              "MeshVertex layout changed — verify shader bindings");

// ─── Submesh ───────────────────────────────────────────────────────────────

/// A contiguous range of indices that shares one material.  The
/// renderer issues one draw call per submesh per pass.  Keeping
/// submesh descriptors tiny means a character with a dozen pieces
/// still fits its metadata into a single cache line.
struct MeshSubmesh
{
    std::uint32_t indexOffset  = 0;
    std::uint32_t indexCount   = 0;
    std::uint64_t materialId   = 0; ///< Resolved by the renderer.
    std::string   debugName;        ///< For the frame capture UI.
};

// ─── Level of detail ───────────────────────────────────────────────────────

/// One LOD of a mesh.  LOD0 is always present; LOD1+ are optional.
/// The streaming manager may unload any LOD > 0 under memory
/// pressure, falling back to the coarsest LOD that is still
/// resident.
struct MeshLod
{
    /// Camera distance (in world units) beyond which this LOD is
    /// preferred.  The renderer picks the LOD whose threshold is
    /// less-than-or-equal-to the actual distance.  LOD0 uses zero.
    float screenDistanceThreshold = 0.0f;

    std::vector<MeshVertex>  vertices;
    std::vector<std::uint32_t> indices;

    /// Submesh ranges inside this LOD's index buffer.  Submesh count
    /// may differ between LODs — coarser LODs often merge small
    /// submeshes to cut draw calls.
    std::vector<MeshSubmesh> submeshes;

    /// Streaming hints.  Populated by the asset importer from the
    /// source mesh so the streaming manager can budget without
    /// decoding the full vector payloads above.
    std::uint32_t vertexCountHint = 0;
    std::uint32_t indexCountHint  = 0;
    std::uint64_t approxGpuBytes  = 0;
};

// ─── Bounding volume ───────────────────────────────────────────────────────

/// Axis-aligned bounding box.  Stored on the mesh root and on each
/// LOD so culling can reject coarse LODs early without touching the
/// per-vertex data.  Centre / half-extents is easier to use in hot
/// paths than min / max because the frustum test reads centre +
/// radius directly.
struct MeshAabb
{
    MeshVec3 centre{};
    MeshVec3 halfExtents{};
};

// ─── Asset root ────────────────────────────────────────────────────────────

/// On-disk / in-memory mesh asset.  A single instance may reference
/// several LODs and several submeshes but always represents one
/// logical object (a character body, a tree, a building prop).
struct MeshAsset
{
    std::uint64_t meshId = 0;
    std::string   debugName;

    /// Source path the importer read.  Diagnostics only — the
    /// runtime looks up meshes by `meshId`.
    std::string sourcePath;

    MeshAabb rootBounds{};
    std::array<MeshLod, 4> lods{}; ///< Fixed size — max four LODs per mesh.
    std::uint8_t           lodCount = 0; ///< How many of the slots are valid.

    /// Flags that persist across the pipeline — compiled-in defaults
    /// can be overridden by the editor on a per-asset basis.
    bool hasUv1        = false;
    bool hasTangents   = true;
    bool castsShadows  = true;
    bool isStatic      = true; ///< False means streaming may tear down LODs more aggressively.
};

// ─── Limits ────────────────────────────────────────────────────────────────

/// Maximum vertices in a single LOD.  Above this the importer should
/// split into submeshes or request a coarser LOD — the hard cap
/// catches pathological imports that would otherwise blow the
/// renderer's per-draw vertex budget.
inline constexpr std::uint32_t kMaxMeshVerticesPerLod = 1'048'576;

/// Maximum indices in a single LOD.  Same rationale — a sentinel
/// that turns importer bugs into visible failures.
inline constexpr std::uint32_t kMaxMeshIndicesPerLod = 3 * kMaxMeshVerticesPerLod;

} // namespace SagaEngine::Render
