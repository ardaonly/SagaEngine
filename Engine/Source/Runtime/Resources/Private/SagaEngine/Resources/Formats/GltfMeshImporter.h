/// @file GltfMeshImporter.h
/// @brief glTF/GLB mesh importer producing `MeshAsset`.
///
/// Layer  : SagaEngine / Resources / Formats
/// Purpose: Reads a `.gltf` (JSON + external buffers) or `.glb`
///          (binary container) file and produces a `Render::MeshAsset`
///          with interleaved vertices, indexed submeshes, and up to
///          four LODs.  The importer is the bridge between the content
///          pipeline (DCC tools export glTF) and the engine's runtime
///          mesh representation.
///
///          This importer runs on the CPU during asset load or during
///          the cook step — it is not a real-time decoder.  The
///          streaming manager feeds it raw bytes from an `IAssetSource`;
///          the importer parses those bytes and returns a populated
///          `MeshAsset`.
///
/// Design rules:
///   - No external glTF library dependency.  The parser is hand-rolled
///     for the subset of the glTF 2.0 spec that the engine actually
///     uses (static meshes with normals, tangents, two UV sets, and
///     indices).  Adding a full tinygltf or cgltf would pull in JSON
///     and image decoding deps that the resources subsystem does not
///     need elsewhere.
///   - GLM-free output.  The importer writes `Render::MeshVertex` and
///     `Render::MeshVec3` types — the same ones in `MeshAsset.h` — so
///     the caller never sees a glm::vec3 in a resource header.
///   - Validation-first.  Every index, stride, and count is checked
///     against the hard caps in `MeshAsset.h` before a single vertex
///     is copied.  Pathological imports become early failures, not
///     heap overflows.
///
/// What this header is NOT:
///   - Not an animation importer.  Skinning, joints, and animation
///     clips ride a sibling header once skeletal animation lands.
///   - Not a material importer.  PBR textures and material parameters
///     are parsed by `GltfMaterialImporter.h` — this file is strictly
///     geometry.

#pragma once

#include "SagaEngine/Render/Materials/MeshAsset.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Resources {

// ─── Import result ─────────────────────────────────────────────────────────

/// Outcome of a glTF import.  Carries the populated `MeshAsset` on
/// success and a diagnostic string on failure.
struct GltfImportResult
{
    bool        success = false;
    Render::MeshAsset mesh;
    std::string diagnostic;
};

// ─── GltfMeshImporter ──────────────────────────────────────────────────────

/// glTF 2.0 static mesh importer.  Stateless — every call is
/// independent, so the same instance can be used from several
/// streaming workers without a mutex.
class GltfMeshImporter
{
public:
    GltfMeshImporter() noexcept  = default;
    ~GltfMeshImporter() noexcept = default;

    // ── Import entry points ───────────────────────────────────────────────

    /// Import from raw glTF/GLB bytes.  The `sourcePath` is used only
    /// for relative buffer references in `.gltf` mode — the importer
    /// does not touch the filesystem for `.glb` because the binary
    /// payload is self-contained.
    ///
    /// Threading: reentrant — no instance state is mutated during
    /// the parse, so several threads can call `Import` on the same
    /// importer concurrently.
    [[nodiscard]] static GltfImportResult ImportFromBytes(
        const std::vector<std::uint8_t>& bytes,
        const std::string&               sourcePath,
        std::uint64_t                    meshId);

    /// Import from a file path.  Convenience wrapper that reads the
    /// file and forwards to `ImportFromBytes`.  Used by the editor
    /// and by the cook pipeline; the streaming manager prefers the
    /// bytes variant because the bytes already came from the
    /// `IAssetSource`.
    [[nodiscard]] static GltfImportResult ImportFromFile(
        const std::string& filePath,
        std::uint64_t      meshId);

private:
    // ─── GLB parsing ──────────────────────────────────────────────────────

    /// GLB header: 12 bytes (magic, version, length).
    [[nodiscard]] static bool ParseGlbHeader(const std::vector<std::uint8_t>& bytes,
                                             std::uint32_t& outVersion,
                                             std::uint32_t& outTotalLength);

    /// Extract the JSON chunk and the BIN chunk from a GLB payload.
    /// Returns false if the GLB structure is invalid.
    [[nodiscard]] static bool ExtractGlbChunks(const std::vector<std::uint8_t>& bytes,
                                               std::vector<std::uint8_t>& outJson,
                                               std::vector<std::uint8_t>& outBin);

    // ─── glTF JSON parsing ────────────────────────────────────────────────

    /// Minimal JSON-to-mesh pipeline.  Parses the glTF JSON structure,
    /// validates accessor counts, extracts vertices and indices from
    /// the binary buffer, and populates the `MeshAsset`.
    [[nodiscard]] static GltfImportResult ParseGltfJson(
        const std::string&               jsonText,
        const std::vector<std::uint8_t>& binData,
        const std::string&               sourcePath,
        std::uint64_t                    meshId);
};

} // namespace SagaEngine::Resources
