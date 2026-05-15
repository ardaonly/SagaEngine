/// @file TextureImporter.cpp
/// @brief DDS/KTX2 texture importer producing RHI-ready mip chains.

#include "SagaEngine/Resources/Formats/TextureImporter.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace SagaEngine::Resources {

namespace {

constexpr const char* kLogTag = "Resources";

// ─── DDS constants ─────────────────────────────────────────────────────────

/// DDS magic: "DDS " (0x20534444).
inline constexpr std::uint32_t kDdsMagic = 0x20534444u;

/// DDS header size (124 bytes for the main struct).
inline constexpr std::size_t kDdsHeaderSize = 124;

/// DDS_PIXELFORMAT size (32 bytes).
inline constexpr std::size_t kDdsPixelFormatSize = 32;

/// DDS flag: texture contains compressed RGB data.
inline constexpr std::uint32_t kDdsPfFlagFourcc = 0x00000004u;

/// DDS flag: texture contains uncompressed RGB data.
inline constexpr std::uint32_t kDdsPfFlagRgb      = 0x00000040u;

/// DDS flag: texture contains uncompressed RGBA data.
inline constexpr std::uint32_t kDdsPfFlagRgba     = 0x00000041u;

/// DDS flag: texture has mip levels.
inline constexpr std::uint32_t kDdsCapsMipMap     = 0x00200000u;

/// DDS flag: valid header.
inline constexpr std::uint32_t kDdsPfFlagValid    = 0x00000008u;

// ─── KTX2 constants ────────────────────────────────────────────────────────

/// KTX2 identifier: "«KTX 20»\r\n\x1A\n" (12 bytes).
inline constexpr std::array<std::uint8_t, 12> kKtx2Identifier = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

// ─── DDS header structure ──────────────────────────────────────────────────

/// Minimal DDS header view — we only read the fields we need.
/// The actual DDS header is 128 bytes (4 magic + 124 struct);
/// we skip the struct definition and read fields by offset to
/// avoid packing/alignment issues across compilers.
struct DdsHeaderView
{
    std::uint32_t size;             ///< 124
    std::uint32_t flags;
    std::uint32_t height;
    std::uint32_t width;
    std::uint32_t pitchOrLinearSize;
    std::uint32_t depth;            ///< For volume textures.
    std::uint32_t mipMapCount;

    // Reserved fields (skipped).
    std::array<std::uint32_t, 11> reserved;

    // Pixel format.
    std::uint32_t pfSize;
    std::uint32_t pfFlags;
    std::uint32_t pfFourCc;
    std::uint32_t pfRgbBitCount;
    std::uint32_t pfRBitMask;
    std::uint32_t pfGBitMask;
    std::uint32_t pfBBitMask;
    std::uint32_t pfABitMask;

    // Caps.
    std::uint32_t caps1;
    std::uint32_t caps2;
    // (caps3, caps4, reserved2 — skipped)
};

/// Read a little-endian uint32.  DDS and KTX2 are both little-endian.
[[nodiscard]] std::uint32_t ReadLe32(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

} // namespace

// ─── DDS FourCC mapping ────────────────────────────────────────────────────

TextureFormat TextureImporter::DdsFourCcToFormat(std::uint32_t fourCc) noexcept
{
    // FourCC codes are 4 ASCII characters packed into a uint32.
    // We compare against the little-endian encoding of the code.
    auto MakeFourCc = [](char a, char b, char c, char d) -> std::uint32_t {
        return static_cast<std::uint32_t>(a)
             | (static_cast<std::uint32_t>(b) << 8)
             | (static_cast<std::uint32_t>(c) << 16)
             | (static_cast<std::uint32_t>(d) << 24);
    };

    if (fourCc == MakeFourCc('D', 'X', 'T', '1')) return TextureFormat::BC1_RGB;
    if (fourCc == MakeFourCc('D', 'X', 'T', '3')) return TextureFormat::BC2;
    if (fourCc == MakeFourCc('D', 'X', 'T', '5')) return TextureFormat::BC3;
    if (fourCc == MakeFourCc('B', 'C', '4', 'U')) return TextureFormat::BC4_Unorm;
    if (fourCc == MakeFourCc('B', 'C', '4', 'S')) return TextureFormat::BC4_Snorm;
    if (fourCc == MakeFourCc('B', 'C', '5', 'U')) return TextureFormat::BC5_Unorm;
    if (fourCc == MakeFourCc('B', 'C', '5', 'S')) return TextureFormat::BC5_Snorm;
    if (fourCc == MakeFourCc('B', 'C', '6', 'H')) return TextureFormat::BC6H_Ufloat;
    if (fourCc == MakeFourCc('B', 'C', '6', 'S')) return TextureFormat::BC6H_Sfloat;
    if (fourCc == MakeFourCc('B', 'C', '7', '\0')) return TextureFormat::BC7;

    return TextureFormat::Unknown;
}

// ─── Compute mip byte size ─────────────────────────────────────────────────

std::uint64_t TextureImporter::ComputeMipByteSize(TextureFormat format,
                                                  std::uint32_t width,
                                                  std::uint32_t height,
                                                  std::uint32_t depth,
                                                  std::uint32_t arraySize) noexcept
{
    // Block-compressed formats use 4x4 blocks (except ASTC which
    // has variable block sizes).
    auto BlockDimensions = [format]() -> std::pair<std::uint32_t, std::uint32_t> {
        switch (format)
        {
            case TextureFormat::ASTC_4x4: return {4, 4};
            case TextureFormat::ASTC_6x6: return {6, 6};
            case TextureFormat::ASTC_8x8: return {8, 8};
            default:                      return {4, 4}; // BC formats.
        }
    };

    auto BlockByteSize = [format]() -> std::uint32_t {
        switch (format)
        {
            case TextureFormat::BC1_RGB:
            case TextureFormat::BC1_RGBA:
            case TextureFormat::BC4_Unorm:
            case TextureFormat::BC4_Snorm:
                return 8;

            case TextureFormat::BC2:
            case TextureFormat::BC3:
            case TextureFormat::BC5_Unorm:
            case TextureFormat::BC5_Snorm:
            case TextureFormat::BC6H_Ufloat:
            case TextureFormat::BC6H_Sfloat:
            case TextureFormat::BC7:
                return 16;

            case TextureFormat::R8_Unorm:   return 1;
            case TextureFormat::R8G8_Unorm: return 2;
            case TextureFormat::R8G8B8A8_Unorm: return 4;

            case TextureFormat::ASTC_4x4: return 16;
            case TextureFormat::ASTC_6x6: return 16;
            case TextureFormat::ASTC_8x8: return 16;

            default: return 0;
        }
    };

    if (format >= TextureFormat::BC1_RGB && format <= TextureFormat::BC7)
    {
        // Block-compressed: round up dimensions to block size.
        auto [bw, bh] = BlockDimensions();
        std::uint32_t blockW = (width + bw - 1) / bw;
        std::uint32_t blockH = (height + bh - 1) / bh;
        return static_cast<std::uint64_t>(blockW) * blockH * depth * arraySize * BlockByteSize();
    }
    else if (format >= TextureFormat::ASTC_4x4 && format <= TextureFormat::ASTC_8x8)
    {
        auto [bw, bh] = BlockDimensions();
        std::uint32_t blockW = (width + bw - 1) / bw;
        std::uint32_t blockH = (height + bh - 1) / bh;
        return static_cast<std::uint64_t>(blockW) * blockH * depth * arraySize * BlockByteSize();
    }
    else
    {
        // Uncompressed: bytes per pixel * dimensions.
        return static_cast<std::uint64_t>(width) * height * depth * arraySize * BlockByteSize();
    }
}

// ─── Parse DDS ──────────────────────────────────────────────────────────────

bool TextureImporter::ParseDds(const std::vector<std::uint8_t>& bytes,
                               TextureAsset& out)
{
    // Check magic.
    if (bytes.size() < 4)
        return false;

    std::uint32_t magic = ReadLe32(bytes.data());
    if (magic != kDdsMagic)
        return false;

    // Check header size.
    if (bytes.size() < 4 + kDdsHeaderSize)
        return false;

    const std::uint8_t* hdr = bytes.data() + 4;

    DdsHeaderView view;
    std::memset(&view, 0, sizeof(view));

    view.size                    = ReadLe32(hdr + 0);
    view.flags                   = ReadLe32(hdr + 4);
    view.height                  = ReadLe32(hdr + 8);
    view.width                   = ReadLe32(hdr + 12);
    view.pitchOrLinearSize       = ReadLe32(hdr + 16);
    view.depth                   = ReadLe32(hdr + 20);
    view.mipMapCount             = ReadLe32(hdr + 24);

    view.pfSize                  = ReadLe32(hdr + 76);
    view.pfFlags                 = ReadLe32(hdr + 80);
    view.pfFourCc                = ReadLe32(hdr + 84);
    view.pfRgbBitCount           = ReadLe32(hdr + 88);
    view.pfRBitMask              = ReadLe32(hdr + 92);
    view.pfGBitMask              = ReadLe32(hdr + 96);
    view.pfBBitMask              = ReadLe32(hdr + 100);
    view.pfABitMask              = ReadLe32(hdr + 104);

    view.caps1                   = ReadLe32(hdr + 108);
    view.caps2                   = ReadLe32(hdr + 112);

    // Validate header.
    if (view.size != kDdsHeaderSize)
        return false;

    if (view.width == 0 || view.height == 0)
        return false;

    // Determine format.
    TextureFormat format = TextureFormat::Unknown;

    if (view.pfFlags & kDdsPfFlagFourcc)
    {
        format = DdsFourCcToFormat(view.pfFourCc);
    }
    else if (view.pfFlags & kDdsPfFlagRgba)
    {
        format = TextureFormat::R8G8B8A8_Unorm;
    }
    else if (view.pfFlags & kDdsPfFlagRgb)
    {
        // Check bit masks to determine format.
        if (view.pfRBitMask == 0x000000FF && view.pfGBitMask == 0x0000FF00 &&
            view.pfBBitMask == 0x00FF0000 && view.pfABitMask == 0xFF000000)
        {
            format = TextureFormat::R8G8B8A8_Unorm;
        }
        else if (view.pfRBitMask == 0x000000FF && view.pfGBitMask == 0x0000FF00 &&
                 view.pfBBitMask == 0x00FF0000 && view.pfABitMask == 0x00000000)
        {
            format = TextureFormat::R8G8B8A8_Unorm; // Treat as RGBA with alpha=1.
        }
    }

    if (format == TextureFormat::Unknown)
    {
        LOG_WARN(kLogTag,
                 "TextureImporter: unsupported DDS format (fourCc=0x%08X, flags=0x%08X)",
                 static_cast<unsigned>(view.pfFourCc),
                 static_cast<unsigned>(view.pfFlags));
        return false;
    }

    // Populate texture asset.
    out.width     = view.width;
    out.height    = view.height;
    out.depth     = view.depth > 0 ? view.depth : 1;
    out.mipLevels = (view.caps1 & kDdsCapsMipMap) ? std::max(view.mipMapCount, 1u) : 1;
    out.arraySize = 1;
    out.format    = format;

    // Extract mip data.
    std::size_t dataOffset = 4 + kDdsHeaderSize;
    out.mipData.resize(out.mipLevels);

    for (std::uint32_t i = 0; i < out.mipLevels; ++i)
    {
        std::uint32_t mipW = std::max(out.width >> i, 1u);
        std::uint32_t mipH = std::max(out.height >> i, 1u);

        std::uint64_t mipSize = ComputeMipByteSize(format, mipW, mipH, out.depth, out.arraySize);

        if (dataOffset + static_cast<std::size_t>(mipSize) > bytes.size())
        {
            LOG_WARN(kLogTag,
                     "TextureImporter: DDS truncated at mip level %u (need %llu bytes, have %zu)",
                     static_cast<unsigned>(i),
                     static_cast<unsigned long long>(mipSize),
                     bytes.size() - dataOffset);
            return false;
        }

        out.mipData[i].assign(bytes.begin() + static_cast<std::ptrdiff_t>(dataOffset),
                              bytes.begin() + static_cast<std::ptrdiff_t>(dataOffset + static_cast<std::size_t>(mipSize)));
        dataOffset += static_cast<std::size_t>(mipSize);
    }

    // Compute GPU footprint.
    out.approxGpuBytes = 0;
    for (const auto& mip : out.mipData)
        out.approxGpuBytes += mip.size();

    return true;
}

// ─── Parse KTX2 ─────────────────────────────────────────────────────────────

bool TextureImporter::ParseKtx2(const std::vector<std::uint8_t>& bytes,
                                TextureAsset& out)
{
    if (bytes.size() < kKtx2Identifier.size())
        return false;

    // Check identifier.
    for (std::size_t i = 0; i < kKtx2Identifier.size(); ++i)
    {
        if (bytes[i] != kKtx2Identifier[i])
            return false;
    }

    // KTX2 header follows the identifier (52 bytes).
    if (bytes.size() < kKtx2Identifier.size() + 52)
        return false;

    const std::uint8_t* hdr = bytes.data() + kKtx2Identifier.size();

    std::uint32_t vkFormat        = ReadLe32(hdr + 0);
    std::uint32_t typeSize        = ReadLe32(hdr + 4);
    std::uint32_t pixelWidth      = ReadLe32(hdr + 8);
    std::uint32_t pixelHeight     = ReadLe32(hdr + 12);
    std::uint32_t pixelDepth      = ReadLe32(hdr + 16);
    std::uint32_t layerCount      = ReadLe32(hdr + 20);
    std::uint32_t faceCount       = ReadLe32(hdr + 24);
    std::uint32_t levelCount      = ReadLe32(hdr + 28);
    // supercompressionScheme — skipped for now (KTX2 supports ZSTD).
    // We only support uncompressed KTX2.

    if (pixelWidth == 0 || pixelHeight == 0)
        return false;

    // Map Vulkan format to our format.  This is a minimal mapping;
    // a production importer would handle all the common GPU formats.
    TextureFormat format = TextureFormat::Unknown;

    // Vulkan format IDs (from vkFormat enum).
    if (vkFormat == 139) format = TextureFormat::BC1_RGB;           // VK_FORMAT_BC1_RGB_SRGB_BLOCK.
    else if (vkFormat == 131) format = TextureFormat::BC1_RGB;       // VK_FORMAT_BC1_RGB_UNORM_BLOCK.
    else if (vkFormat == 140) format = TextureFormat::BC1_RGBA;      // VK_FORMAT_BC1_RGBA_SRGB_BLOCK.
    else if (vkFormat == 132) format = TextureFormat::BC1_RGBA;      // VK_FORMAT_BC1_RGBA_UNORM_BLOCK.
    else if (vkFormat == 142) format = TextureFormat::BC2;           // VK_FORMAT_BC2_SRGB_BLOCK.
    else if (vkFormat == 134) format = TextureFormat::BC2;           // VK_FORMAT_BC2_UNORM_BLOCK.
    else if (vkFormat == 144) format = TextureFormat::BC3;           // VK_FORMAT_BC3_SRGB_BLOCK.
    else if (vkFormat == 136) format = TextureFormat::BC3;           // VK_FORMAT_BC3_UNORM_BLOCK.
    else if (vkFormat == 148) format = TextureFormat::BC7;           // VK_FORMAT_BC7_SRGB_BLOCK.
    else if (vkFormat == 146) format = TextureFormat::BC7;           // VK_FORMAT_BC7_UNORM_BLOCK.
    else if (vkFormat == 43)  format = TextureFormat::R8G8B8A8_Unorm; // VK_FORMAT_R8G8B8A8_UNORM.
    else if (vkFormat == 44)  format = TextureFormat::R8G8B8A8_Unorm; // VK_FORMAT_R8G8B8A8_SRGB.

    if (format == TextureFormat::Unknown)
    {
        LOG_WARN(kLogTag,
                 "TextureImporter: unsupported KTX2 vkFormat %u",
                 static_cast<unsigned>(vkFormat));
        return false;
    }

    // Populate texture asset.
    out.width     = pixelWidth;
    out.height    = pixelHeight;
    out.depth     = pixelDepth > 0 ? pixelDepth : 1;
    out.mipLevels = levelCount > 0 ? levelCount : 1;
    out.arraySize = layerCount > 0 ? layerCount : 1;
    out.isCubeMap = faceCount == 6;
    out.format    = format;
    out.isSrgb    = (vkFormat == 139 || vkFormat == 140 || vkFormat == 142 ||
                     vkFormat == 144 || vkFormat == 148 || vkFormat == 44);

    // KTX2 level data follows the header + key-value data.
    // For simplicity, we skip the KV data and read the level index.
    std::size_t offset = kKtx2Identifier.size() + 52;

    // Skip key-value data (length-prefixed).
    // keyValueLen is a uint64 at offset 48 within the 52-byte header.
    if (offset + 8 <= bytes.size())
    {
        std::uint64_t kvDataLen = ReadLe32(hdr + 48) | (static_cast<std::uint64_t>(ReadLe32(hdr + 52)) << 32);
        offset += static_cast<std::size_t>(kvDataLen);
        // KTX2 requires the kvDataLen itself to be padded to 4 bytes.
        offset = (offset + 3) & ~static_cast<std::size_t>(3);
    }

    // Level index: each level has a 3-element array of uint64 byte counts
    // (for cubemap faces / array layers).  For simple 2D textures, only
    // the first entry is non-zero.  KTX2 stores these as little-endian
    // uint64 values, so we read all 8 bytes properly.
    out.mipData.resize(out.mipLevels);
    out.approxGpuBytes = 0;

    for (std::uint32_t i = 0; i < out.mipLevels; ++i)
    {
        // Each level entry has 3 x uint64 byte counts (24 bytes total).
        if (offset + 24 > bytes.size())
            return false;

        // Read byte counts for this level.  KTX2 uses uint64 for each
        // face/layer size even though simple 2D textures only use the
        // first slot.
        std::uint64_t bCount[3];
        for (int j = 0; j < 3; ++j)
        {
            const std::uint8_t* p = bytes.data() + offset;
            bCount[j] = static_cast<std::uint64_t>(ReadLe32(p))
                      | (static_cast<std::uint64_t>(ReadLe32(p + 4)) << 32);
            offset += 8;
        }

        std::uint64_t mipSize64 = bCount[0];
        if (offset + static_cast<std::size_t>(mipSize64) > bytes.size())
            return false;

        std::size_t mipSize = static_cast<std::size_t>(mipSize64);
        out.mipData[i].assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                              bytes.begin() + static_cast<std::ptrdiff_t>(offset + mipSize));
        offset += mipSize;
        out.approxGpuBytes += mipSize;

        // Align to 4 bytes per KTX2 spec.
        offset = (offset + 3) & ~static_cast<std::size_t>(3);
    }

    return true;
}

// ─── ImportFromBytes ───────────────────────────────────────────────────────

TextureImportResult TextureImporter::ImportFromBytes(
    const std::vector<std::uint8_t>& bytes,
    const std::string&               sourcePath,
    std::uint64_t                    textureId)
{
    if (bytes.empty())
        return {false, {}, "TextureImporter: empty byte input"};

    TextureAsset asset;
    asset.textureId  = textureId;
    asset.debugName  = sourcePath;
    asset.sourcePath = sourcePath;

    bool ok = false;

    // Detect format by magic.
    if (bytes.size() >= 4)
    {
        std::uint32_t magic = ReadLe32(bytes.data());
        if (magic == kDdsMagic)
            ok = ParseDds(bytes, asset);
    }

    if (!ok && bytes.size() >= kKtx2Identifier.size())
    {
        bool isKtx2 = true;
        for (std::size_t i = 0; i < kKtx2Identifier.size(); ++i)
        {
            if (bytes[i] != kKtx2Identifier[i])
            {
                isKtx2 = false;
                break;
            }
        }
        if (isKtx2)
            ok = ParseKtx2(bytes, asset);
    }

    if (!ok)
        return {false, {}, "TextureImporter: unrecognized texture format"};

    return {true, std::move(asset), {}};
}

// ─── ImportFromFile ────────────────────────────────────────────────────────

TextureImportResult TextureImporter::ImportFromFile(
    const std::string& filePath,
    std::uint64_t      textureId)
{
    std::FILE* file = std::fopen(filePath.c_str(), "rb");
    if (!file)
        return {false, {}, "TextureImporter: cannot open file '" + filePath + "'"};

    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::rewind(file);

    if (fileSize <= 0)
    {
        std::fclose(file);
        return {false, {}, "TextureImporter: empty file '" + filePath + "'"};
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    if (std::fread(bytes.data(), 1, static_cast<std::size_t>(fileSize), file) != static_cast<std::size_t>(fileSize))
    {
        std::fclose(file);
        return {false, {}, "TextureImporter: failed to read '" + filePath + "'"};
    }
    std::fclose(file);

    return ImportFromBytes(bytes, filePath, textureId);
}

} // namespace SagaEngine::Resources
