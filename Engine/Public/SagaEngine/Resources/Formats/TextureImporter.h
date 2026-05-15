/// @file TextureImporter.h
/// @brief DDS/KTX2 texture importer producing RHI-ready mip chains.
///
/// Layer  : SagaEngine / Resources / Formats
/// Purpose: Reads a `.dds` (DirectDraw Surface) or `.ktx2` (Khronos
///          Texture) file and produces a texture description ready
///          for GPU upload.  Both formats are industry-standard
///          compressed texture containers that ship with pre-built
///          mip chains and GPU-compressed block formats (BC1-BC7,
///          ASTC, ETC2).  The importer extracts the mip levels,
///          format metadata, and pixel data so the renderer can
///          upload them to the RHI without a second copy or a
///          runtime transcode.
///
///          This importer runs during asset load or the cook step.
///          The streaming manager feeds it raw bytes from an
///          `IAssetSource`; the importer parses those bytes and
///          returns a populated `TextureAsset`.
///
/// Design rules:
///   - Zero-copy where possible.  The importer validates the header
///     and returns a view into the source bytes — the RHI upload
///     code reads directly from the streaming payload.
///   - No external texture library dependency.  DDS is a simple
///     fixed-header format; KTX2 is a key-value container.  Both
///     can be parsed with a few hundred lines of code and no
///     transitive includes.
///   - Format passthrough.  The importer does not transcode between
///     block formats — if the DDS contains BC3, the RHI gets BC3.
///     Transcoding (e.g. BC3 → ASTC for mobile) is a cook-time
///     operation.
///
/// What this header is NOT:
///   - Not an image decoder.  PNG/JPEG decoding happens at cook
///     time; runtime textures are already in GPU-compressed formats.
///   - Not a texture atlas manager.  Packing multiple textures into
///     one atlas is a separate toolchain step.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Resources {

// ─── Texture format ────────────────────────────────────────────────────────

/// GPU texture format.  Mirrors the block-compressed formats that
/// the RHI supports; extended as new platforms land.
enum class TextureFormat : std::uint8_t
{
    Unknown = 0,

    // BC (Block Compression) — desktop / console.
    BC1_RGB,            ///< DXT1 / RGB, 4x4 blocks, 8 bytes/block.
    BC1_RGBA,           ///< DXT1 / RGBA, 4x4 blocks, 8 bytes/block.
    BC2,                ///< DXT3, 4x4 blocks, 16 bytes/block.
    BC3,                ///< DXT5, 4x4 blocks, 16 bytes/block.
    BC4_Unorm,          ///< RGTC1 red, 4x4 blocks, 8 bytes/block.
    BC4_Snorm,          ///< RGTC1 red signed.
    BC5_Unorm,          ///< RGTC2 red-green, 4x4 blocks, 16 bytes/block.
    BC5_Snorm,          ///< RGTC2 red-green signed.
    BC6H_Ufloat,        ///< BPTC unsigned float.
    BC6H_Sfloat,        ///< BPTC signed float.
    BC7,                ///< BPTC high-quality RGBA.

    // Uncompressed.
    R8_Unorm,           ///< 8-bit red.
    R8G8_Unorm,         ///< 8-bit RG.
    R8G8B8A8_Unorm,     ///< 32-bit RGBA (common for UI / dynamic textures).

    // ASTC — mobile (Android / iOS).
    ASTC_4x4,
    ASTC_6x6,
    ASTC_8x8,
};

// ─── Texture asset ─────────────────────────────────────────────────────────

/// CPU-side texture description.  Populated by the importer;
/// consumed by the RHI upload pipeline.
struct TextureAsset
{
    std::uint64_t textureId = 0;
    std::string   debugName;
    std::string   sourcePath;

    TextureFormat format = TextureFormat::Unknown;

    std::uint32_t width     = 0;
    std::uint32_t height    = 0;
    std::uint32_t depth     = 1; ///< >1 for 3D textures, 1 otherwise.
    std::uint32_t mipLevels = 1;
    std::uint32_t arraySize = 1; ///< >1 for texture arrays.

    /// Whether the texture is a cube map.  Cube maps have
    /// `arraySize == 6` (one face per array element) or
    /// `arraySize == 6 * N` for cube array textures.
    bool isCubeMap = false;

    /// Whether the texture data is sRGB.  The RHI uses this to pick
    /// the correct sampler state and format.
    bool isSrgb = false;

    /// Mip chain data.  `mipData[i]` holds the raw bytes for mip
    /// level `i` (where level 0 is the full-resolution image).  The
    /// importer populates these by slicing the DDS/KTX2 payload.
    std::vector<std::vector<std::uint8_t>> mipData;

    /// Approximate GPU memory footprint in bytes.  Computed from
    /// dimensions and format block size so the streaming budget
    /// tracker can estimate residency without decoding the full
    /// payload.
    std::uint64_t approxGpuBytes = 0;
};

// ─── Import result ─────────────────────────────────────────────────────────

/// Outcome of a texture import.  Carries the populated `TextureAsset`
/// on success and a diagnostic string on failure.
struct TextureImportResult
{
    bool              success = false;
    TextureAsset      texture;
    std::string       diagnostic;
};

// ─── TextureImporter ───────────────────────────────────────────────────────

/// DDS/KTX2 texture importer.  Stateless — every call is
/// independent, so the same instance can be used from several
/// streaming workers without a mutex.
class TextureImporter
{
public:
    TextureImporter() noexcept  = default;
    ~TextureImporter() noexcept = default;

    // ── Import entry points ───────────────────────────────────────────────

    /// Import from raw DDS/KTX2 bytes.  The `sourcePath` is used only
    /// for the debug name — the importer does not touch the
    /// filesystem.
    ///
    /// Threading: reentrant — no instance state is mutated during
    /// the parse, so several threads can call `ImportFromBytes` on
    /// the same importer concurrently.
    [[nodiscard]] static TextureImportResult ImportFromBytes(
        const std::vector<std::uint8_t>& bytes,
        const std::string&               sourcePath,
        std::uint64_t                    textureId);

    /// Import from a file path.  Convenience wrapper that reads the
    /// file and forwards to `ImportFromBytes`.  Used by the editor
    /// and by the cook pipeline.
    [[nodiscard]] static TextureImportResult ImportFromFile(
        const std::string& filePath,
        std::uint64_t      textureId);

private:
    // ─── DDS parsing ──────────────────────────────────────────────────────

    /// Parse a DDS file.  Returns false if the magic header is wrong
    /// or the pixel format is unsupported.
    [[nodiscard]] static bool ParseDds(const std::vector<std::uint8_t>& bytes,
                                       TextureAsset& out);

    // ─── KTX2 parsing ─────────────────────────────────────────────────────

    /// Parse a KTX2 file.  Returns false if the identifier is wrong
    /// or the vkFormat is not one we support.
    [[nodiscard]] static bool ParseKtx2(const std::vector<std::uint8_t>& bytes,
                                        TextureAsset& out);

    // ─── Format mapping ───────────────────────────────────────────────────

    /// Map a DDS FourCC to our `TextureFormat`.
    [[nodiscard]] static TextureFormat DdsFourCcToFormat(std::uint32_t fourCc) noexcept;

    /// Compute the byte size of one mip level given the format and
    /// dimensions.  Block-compressed formats round up to the nearest
    /// block boundary.
    [[nodiscard]] static std::uint64_t ComputeMipByteSize(TextureFormat format,
                                                          std::uint32_t width,
                                                          std::uint32_t height,
                                                          std::uint32_t depth,
                                                          std::uint32_t arraySize) noexcept;
};

} // namespace SagaEngine::Resources
