/// @file GltfMeshImporter.cpp
/// @brief glTF/GLB mesh importer producing `MeshAsset`.

#include "SagaEngine/Resources/Formats/GltfMeshImporter.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace SagaEngine::Resources {

namespace {

constexpr const char* kLogTag = "Resources";

// ─── GLB constants ─────────────────────────────────────────────────────────

inline constexpr std::uint32_t kGlbMagic = 0x46546C67u; // "glTF"
inline constexpr std::uint32_t kGlbJsonChunkType = 0x4E4F534Au; // "JSON"
inline constexpr std::uint32_t kGlbBinChunkType  = 0x004E4942u; // "BIN\0"

// ─── glTF JSON helpers ─────────────────────────────────────────────────────

/// Minimal JSON value type we care about.
enum class JsonType { Null, Bool, Number, String, Array, Object };

/// A tiny JSON value view into a string.  This is not a full JSON
/// DOM — it's just enough to extract the glTF fields we need without
/// pulling in a third-party library.
struct JsonValue
{
    JsonType            type     = JsonType::Null;
    const char*         start    = nullptr;
    std::size_t         length   = 0;

    /// Number value (parsed on demand).
    [[nodiscard]] double AsNumber() const noexcept
    {
        if (type != JsonType::Number)
            return 0.0;
        char buf[64] = {};
        std::size_t len = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
        std::memcpy(buf, start, len);
        buf[len] = '\0';
        return std::atof(buf);
    }

    [[nodiscard]] int AsInt() const noexcept { return static_cast<int>(AsNumber()); }
    [[nodiscard]] std::uint32_t AsUint32() const noexcept { return static_cast<std::uint32_t>(AsNumber()); }

    /// String value (without quotes).
    [[nodiscard]] std::string AsString() const
    {
        if (type != JsonType::String || length < 2)
            return {};
        return std::string(start + 1, length - 2);
    }
};

/// Skip whitespace.
[[nodiscard]] const char* SkipJsonWs(const char* p) noexcept
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        ++p;
    return p;
}

/// Parse one JSON value (shallow — does not recurse into nested
/// objects/arrays beyond tracking their extent).  Returns the end
/// pointer of the value, or nullptr on error.
[[nodiscard]] const char* ParseJsonValue(const char* p, JsonValue& out) noexcept
{
    p = SkipJsonWs(p);
    out.start = p;

    if (*p == '{')
    {
        // Object — scan to matching brace.
        int depth = 1;
        ++p;
        bool inString = false;
        while (*p && depth > 0)
        {
            if (inString)
            {
                if (*p == '\\' && *(p + 1))
                    ++p;
                else if (*p == '"')
                    inString = false;
            }
            else
            {
                if (*p == '"')
                    inString = true;
                else if (*p == '{')
                    ++depth;
                else if (*p == '}')
                    --depth;
            }
            ++p;
        }
        out.type   = JsonType::Object;
        out.length = static_cast<std::size_t>(p - out.start);
        return p;
    }
    else if (*p == '[')
    {
        // Array — scan to matching bracket.
        int depth = 1;
        ++p;
        bool inString = false;
        while (*p && depth > 0)
        {
            if (inString)
            {
                if (*p == '\\' && *(p + 1))
                    ++p;
                else if (*p == '"')
                    inString = false;
            }
            else
            {
                if (*p == '"')
                    inString = true;
                else if (*p == '[')
                    ++depth;
                else if (*p == ']')
                    --depth;
            }
            ++p;
        }
        out.type   = JsonType::Array;
        out.length = static_cast<std::size_t>(p - out.start);
        return p;
    }
    else if (*p == '"')
    {
        // String.
        ++p;
        while (*p && *p != '"')
        {
            if (*p == '\\' && *(p + 1))
                ++p;
            ++p;
        }
        if (*p == '"')
            ++p;
        out.type   = JsonType::String;
        out.length = static_cast<std::size_t>(p - out.start);
        return p;
    }
    else if (*p == 't' || *p == 'f')
    {
        // Boolean.
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n')
            ++p;
        out.type   = JsonType::Bool;
        out.length = static_cast<std::size_t>(p - out.start);
        return p;
    }
    else if (*p == 'n')
    {
        // Null.
        if (std::strncmp(p, "null", 4) == 0)
            p += 4;
        out.type   = JsonType::Null;
        out.length = static_cast<std::size_t>(p - out.start);
        return p;
    }
    else if (*p == '-' || (*p >= '0' && *p <= '9'))
    {
        // Number.
        if (*p == '-') ++p;
        while (*p >= '0' && *p <= '9') ++p;
        if (*p == '.') { ++p; while (*p >= '0' && *p <= '9') ++p; }
        if (*p == 'e' || *p == 'E') { ++p; if (*p == '+' || *p == '-') ++p; while (*p >= '0' && *p <= '9') ++p; }
        out.type   = JsonType::Number;
        out.length = static_cast<std::size_t>(p - out.start);
        return p;
    }

    return nullptr;
}

/// Find a key in a JSON object string.  Returns pointer to the value
/// start (after the colon), or nullptr if not found.  This is a
/// simple linear scan — good enough for the glTF top-level keys.
[[nodiscard]] const char* FindJsonKey(const char* objectStart,
                                      std::size_t objectLength,
                                      const char* key) noexcept
{
    const std::string quotedKey = std::string("\"") + key + "\":";
    const char* p = objectStart;
    const char* end = objectStart + objectLength;

    while (p < end)
    {
        p = SkipJsonWs(p);
        if (p >= end)
            return nullptr;

        // Check if this is the key we want.
        if (std::strncmp(p, quotedKey.c_str(), quotedKey.size()) == 0)
            return SkipJsonWs(p + quotedKey.size());

        // Skip this key-value pair.
        JsonValue val;
        const char* next = ParseJsonValue(p, val);
        if (!next)
            return nullptr;

        // Skip comma.
        p = SkipJsonWs(next);
        if (*p == ',')
            ++p;
    }

    return nullptr;
}

/// Extract a JSON array of numbers into a vector.
[[nodiscard]] bool ExtractNumberArray(const char* arrayStart,
                                      std::vector<float>& out) noexcept
{
    // Skip opening bracket.
    const char* p = SkipJsonWs(arrayStart);
    if (*p != '[')
        return false;
    ++p;

    out.clear();
    while (true)
    {
        p = SkipJsonWs(p);
        if (*p == ']')
            return true;
        if (*p == ',')
        {
            ++p;
            continue;
        }

        JsonValue val;
        const char* next = ParseJsonValue(p, val);
        if (!next || val.type != JsonType::Number)
            return false;

        out.push_back(static_cast<float>(val.AsNumber()));
        p = next;
    }
}

/// Extract a JSON array of integers into a vector.
[[nodiscard]] bool ExtractUint32Array(const char* arrayStart,
                                      std::vector<std::uint32_t>& out) noexcept
{
    const char* p = SkipJsonWs(arrayStart);
    if (*p != '[')
        return false;
    ++p;

    out.clear();
    while (true)
    {
        p = SkipJsonWs(p);
        if (*p == ']')
            return true;
        if (*p == ',')
        {
            ++p;
            continue;
        }

        JsonValue val;
        const char* next = ParseJsonValue(p, val);
        if (!next || val.type != JsonType::Number)
            return false;

        out.push_back(val.AsUint32());
        p = next;
    }
}

/// Find the Nth element in a JSON array string.  Returns pointer to
/// the element's JSON value, or nullptr.
[[nodiscard]] const char* FindArrayElement(const char* arrayStart,
                                           std::uint32_t index) noexcept
{
    const char* p = SkipJsonWs(arrayStart);
    if (*p != '[')
        return nullptr;
    ++p;

    for (std::uint32_t i = 0; ; ++i)
    {
        p = SkipJsonWs(p);
        if (!*p || *p == ']')
            return nullptr;
        if (*p == ',')
        {
            ++p;
            continue;
        }

        if (i == index)
            return p;

        JsonValue val;
        const char* next = ParseJsonValue(p, val);
        if (!next)
            return nullptr;
        p = next;
    }
}

/// Extract a vec3 from a JSON number array.
[[nodiscard]] bool ExtractVec3(const char* arrayStart, Render::MeshVec3& out) noexcept
{
    std::vector<float> vals;
    if (!ExtractNumberArray(arrayStart, vals) || vals.size() < 3)
        return false;
    out.x = vals[0];
    out.y = vals[1];
    out.z = vals[2];
    return true;
}

} // namespace

// ─── GLB Header ────────────────────────────────────────────────────────────

bool GltfMeshImporter::ParseGlbHeader(const std::vector<std::uint8_t>& bytes,
                                      std::uint32_t& outVersion,
                                      std::uint32_t& outTotalLength)
{
    if (bytes.size() < 12)
        return false;

    // Read magic.
    std::uint32_t magic = 0;
    std::memcpy(&magic, bytes.data(), 4);
    if (magic != kGlbMagic)
        return false;

    // Read version.
    std::memcpy(&outVersion, bytes.data() + 4, 4);

    // Read total length.
    std::memcpy(&outTotalLength, bytes.data() + 8, 4);

    if (outTotalLength > bytes.size())
        return false;

    return true;
}

// ─── Extract GLB Chunks ────────────────────────────────────────────────────

bool GltfMeshImporter::ExtractGlbChunks(const std::vector<std::uint8_t>& bytes,
                                        std::vector<std::uint8_t>& outJson,
                                        std::vector<std::uint8_t>& outBin)
{
    std::uint32_t version = 0, totalLength = 0;
    if (!ParseGlbHeader(bytes, version, totalLength))
        return false;

    // Parse chunks starting after the 12-byte header.
    std::size_t offset = 12;

    while (offset + 8 <= totalLength)
    {
        std::uint32_t chunkLength = 0;
        std::uint32_t chunkType   = 0;
        std::memcpy(&chunkLength, bytes.data() + offset, 4);
        std::memcpy(&chunkType,   bytes.data() + offset + 4, 4);
        offset += 8;

        if (offset + chunkLength > totalLength)
            return false;

        if (chunkType == kGlbJsonChunkType)
        {
            outJson.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                           bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
            // Null-terminate for safe string parsing.
            outJson.push_back('\0');
        }
        else if (chunkType == kGlbBinChunkType)
        {
            outBin.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                          bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
        }

        offset += chunkLength;
    }

    return !outJson.empty();
}

// ─── Parse glTF JSON ───────────────────────────────────────────────────────

GltfImportResult GltfMeshImporter::ParseGltfJson(
    const std::string&               jsonText,
    const std::vector<std::uint8_t>& binData,
    const std::string&               sourcePath,
    std::uint64_t                    meshId)
{
    GltfImportResult result;
    result.success = false;

    const char* root = jsonText.c_str();
    JsonValue rootVal;
    ParseJsonValue(root, rootVal);

    if (rootVal.type != JsonType::Object)
    {
        result.diagnostic = "GltfMeshImporter: root is not a JSON object";
        return result;
    }

    // ─── Extract scenes, scene, nodes, mesh indices ────────────────────────
    // For simplicity, we grab the first mesh from the first node of the
    // default scene.  A full importer would handle the entire scene graph,
    // but for an MMO engine we typically import one mesh at a time.

    const char* scenesVal = FindJsonKey(rootVal.start, rootVal.length, "scenes");
    const char* meshesVal = FindJsonKey(rootVal.start, rootVal.length, "meshes");
    const char* accessorsVal = FindJsonKey(rootVal.start, rootVal.length, "accessors");
    const char* bufferViewsVal = FindJsonKey(rootVal.start, rootVal.length, "bufferViews");

    if (!meshesVal)
    {
        result.diagnostic = "GltfMeshImporter: no 'meshes' key in glTF";
        return result;
    }

    // Get first mesh.
    const char* mesh0 = FindArrayElement(meshesVal, 0);
    if (!mesh0)
    {
        result.diagnostic = "GltfMeshImporter: no meshes in glTF";
        return result;
    }

    JsonValue meshVal;
    ParseJsonValue(mesh0, meshVal);
    if (meshVal.type != JsonType::Object)
    {
        result.diagnostic = "GltfMeshImporter: mesh[0] is not an object";
        return result;
    }

    // Setup mesh asset.
    auto& asset         = result.mesh;
    asset.meshId        = meshId;
    asset.debugName     = sourcePath;
    asset.sourcePath    = sourcePath;
    asset.lodCount      = 1;
    asset.hasTangents   = true;
    asset.isStatic      = true;
    asset.castsShadows  = true;

    // ─── Extract primitives ────────────────────────────────────────────────
    const char* primitivesVal = FindJsonKey(meshVal.start, meshVal.length, "primitives");
    if (!primitivesVal)
    {
        result.diagnostic = "GltfMeshImporter: no 'primitives' in mesh[0]";
        return result;
    }

    JsonValue primitivesArr;
    ParseJsonValue(primitivesVal, primitivesArr);
    if (primitivesArr.type != JsonType::Array)
    {
        result.diagnostic = "GltfMeshImporter: 'primitives' is not an array";
        return result;
    }

    // We support one primitive (submesh) for now.
    const char* prim0 = FindArrayElement(primitivesVal, 0);
    if (!prim0)
    {
        result.diagnostic = "GltfMeshImporter: no primitives in mesh[0]";
        return result;
    }

    // Get attributes.
    const char* attrs = FindJsonKey(prim0, 256, "attributes");
    if (!attrs)
    {
        result.diagnostic = "GltfMeshImporter: no 'attributes' in primitive";
        return result;
    }

    // Extract accessor indices from attributes.
    const char* posAcc = FindJsonKey(attrs, 128, "POSITION");
    const char* normAcc = FindJsonKey(attrs, 128, "NORMAL");
    const char* tanAcc  = FindJsonKey(attrs, 128, "TANGENT");
    const char* uv0Acc  = FindJsonKey(attrs, 128, "TEXCOORD_0");
    if (!posAcc || !normAcc)
    {
        result.diagnostic = "GltfMeshImporter: missing POSITION or NORMAL accessor";
        return result;
    }

    // Extract accessor indices from attributes.  Each value is a
    // JSON number (e.g. "POSITION": 0) — we parse the number at
    // the pointer returned by FindJsonKey, which points just after
    // the colon.
    std::uint32_t posAccIdx  = 0;
    std::uint32_t normAccIdx = 0;
    std::uint32_t tanAccIdx  = 0xFFFFFFFFu;
    std::uint32_t uv0AccIdx  = 0xFFFFFFFFu;

    if (posAcc)
    {
        JsonValue val;
        ParseJsonValue(posAcc, val);
        posAccIdx = (val.type == JsonType::Number) ? val.AsUint32() : 0;
    }
    if (normAcc)
    {
        JsonValue val;
        ParseJsonValue(normAcc, val);
        normAccIdx = (val.type == JsonType::Number) ? val.AsUint32() : 0;
    }
    if (tanAcc)
    {
        JsonValue val;
        ParseJsonValue(tanAcc, val);
        tanAccIdx = (val.type == JsonType::Number) ? val.AsUint32() : 0xFFFFFFFFu;
    }
    if (uv0Acc)
    {
        JsonValue val;
        ParseJsonValue(uv0Acc, val);
        uv0AccIdx = (val.type == JsonType::Number) ? val.AsUint32() : 0xFFFFFFFFu;
    }

    // Get indices accessor.
    const char* indicesVal = FindJsonKey(prim0, 256, "indices");
    std::uint32_t indicesAccIdx = 0xFFFFFFFFu;
    if (indicesVal)
    {
        JsonValue val;
        ParseJsonValue(indicesVal, val);
        indicesAccIdx = (val.type == JsonType::Number) ? val.AsUint32() : 0xFFFFFFFFu;
    }

    // Parse accessor details (count, type, bufferView, byteOffset).
    // For a production importer, we'd parse the full accessor structure.
    // Here, we use a simplified approach assuming standard layouts.

    // Find the accessor objects.
    const char* acc0 = FindArrayElement(accessorsVal, posAccIdx);
    if (!acc0)
    {
        result.diagnostic = "GltfMeshImporter: cannot find POSITION accessor object";
        return result;
    }

    const char* posCountVal = FindJsonKey(acc0, 256, "count");
    std::uint32_t vertexCount = 0;
    if (posCountVal)
    {
        JsonValue val;
        ParseJsonValue(posCountVal, val);
        vertexCount = (val.type == JsonType::Number) ? val.AsUint32() : 0;
    }

    if (vertexCount == 0 || vertexCount > Render::kMaxMeshVerticesPerLod)
    {
        result.diagnostic = "GltfMeshImporter: invalid vertex count " + std::to_string(vertexCount);
        return result;
    }

    // Parse accessor → bufferView → buffer chain to extract real
    // vertex data from the GLB BIN chunk.  Each accessor points at
    // a bufferView, which in turn points at a buffer with a byte
    // offset and stride.  We assume a single BIN chunk (buffer 0).
    auto ExtractAccessorData = [&](const char* accJson,
                                   std::uint32_t accessorIdx,
                                   std::uint32_t count,
                                   std::uint32_t componentType,
                                   std::uint32_t expectedComponents) -> std::vector<float> {
        std::vector<float> out;
        if (!accJson || binData.empty())
            return out;

        // Find the bufferView for this accessor.
        const char* bvVal = FindJsonKey(accJson, 256, "bufferView");
        if (!bvVal)
            return out;

        JsonValue bvIdxVal;
        ParseJsonValue(bvVal, bvIdxVal);
        if (bvIdxVal.type != JsonType::Number)
            return out;

        std::uint32_t bvIdx = bvIdxVal.AsUint32();
        const char* bvJson = FindArrayElement(bufferViewsVal, bvIdx);
        if (!bvJson)
            return out;

        // Get buffer, byteOffset, and byteStride from the bufferView.
        const char* bufVal = FindJsonKey(bvJson, 256, "buffer");
        const char* offVal = FindJsonKey(bvJson, 256, "byteOffset");
        const char* strVal = FindJsonKey(bvJson, 256, "byteStride");

        std::uint32_t bufIdx = 0;
        if (bufVal)
        {
            JsonValue val;
            ParseJsonValue(bufVal, val);
            bufIdx = (val.type == JsonType::Number) ? val.AsUint32() : 0;
        }
        std::uint32_t offset = 0;
        if (offVal)
        {
            JsonValue val;
            ParseJsonValue(offVal, val);
            offset = (val.type == JsonType::Number) ? val.AsUint32() : 0;
        }
        std::uint32_t stride = 0;
        if (strVal)
        {
            JsonValue val;
            ParseJsonValue(strVal, val);
            stride = (val.type == JsonType::Number) ? val.AsUint32() : 0;
        }
        (void)bufIdx; // GLB only has one buffer — index 0.
        (void)accessorIdx; // Unused — kept for signature clarity.
        (void)componentType; // We assume GL_FLOAT (5126) throughout.

        // If no explicit stride, compute from component count.
        if (stride == 0)
            stride = expectedComponents * 4; // 4 bytes per float.

        // Read the data from the BIN chunk.
        const std::size_t totalBytes = count * stride;
        if (offset + totalBytes > binData.size())
            return out;

        out.resize(static_cast<std::size_t>(count) * expectedComponents);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const std::uint8_t* elemStart = binData.data() + offset + static_cast<std::size_t>(i) * stride;
            for (std::uint32_t c = 0; c < expectedComponents; ++c)
            {
                float val = 0.0f;
                std::memcpy(&val, elemStart + static_cast<std::size_t>(c) * 4, 4);
                out[static_cast<std::size_t>(i) * expectedComponents + c] = val;
            }
        }

        return out;
    };

    // Parse accessor component type (5126 = GL_FLOAT).
    auto GetAccessorComponentType = [&](const char* accJson) -> std::uint32_t {
        const char* compVal = FindJsonKey(accJson, 256, "componentType");
        if (compVal)
        {
            JsonValue val;
            ParseJsonValue(compVal, val);
            if (val.type == JsonType::Number)
                return val.AsUint32();
        }
        return 5126; // Default to GL_FLOAT.
    };

    // Extract POSITION data (3 components: x, y, z).
    std::uint32_t posCompType = GetAccessorComponentType(acc0);
    std::vector<float> positions = ExtractAccessorData(acc0, posAccIdx, vertexCount, posCompType, 3);

    // Extract NORMAL data (3 components: x, y, z).
    const char* normAccJson = FindArrayElement(accessorsVal, normAccIdx);
    std::vector<float> normals;
    if (normAccJson)
    {
        std::uint32_t normCompType = GetAccessorComponentType(normAccJson);
        const char* normCountVal = FindJsonKey(normAccJson, 256, "count");
        std::uint32_t normCount = 0;
        if (normCountVal)
        {
            JsonValue val;
            ParseJsonValue(normCountVal, val);
            normCount = (val.type == JsonType::Number) ? val.AsUint32() : 0;
        }
        normals = ExtractAccessorData(normAccJson, normAccIdx, normCount, normCompType, 3);
    }

    // Extract TANGENT data (4 components: x, y, z, w).
    std::vector<float> tangents;
    if (tanAccIdx != 0xFFFFFFFFu)
    {
        const char* tanAccJson = FindArrayElement(accessorsVal, tanAccIdx);
        if (tanAccJson)
        {
            std::uint32_t tanCompType = GetAccessorComponentType(tanAccJson);
            const char* tanCountVal = FindJsonKey(tanAccJson, 256, "count");
            std::uint32_t tanCount = 0;
            if (tanCountVal)
            {
                JsonValue val;
                ParseJsonValue(tanCountVal, val);
                tanCount = (val.type == JsonType::Number) ? val.AsUint32() : 0;
            }
            tangents = ExtractAccessorData(tanAccJson, tanAccIdx, tanCount, tanCompType, 4);
        }
    }

    // Extract TEXCOORD_0 data (2 components: u, v).
    std::vector<float> uv0Data;
    if (uv0AccIdx != 0xFFFFFFFFu)
    {
        const char* uv0AccJson = FindArrayElement(accessorsVal, uv0AccIdx);
        if (uv0AccJson)
        {
            std::uint32_t uv0CompType = GetAccessorComponentType(uv0AccJson);
            const char* uv0CountVal = FindJsonKey(uv0AccJson, 256, "count");
            std::uint32_t uv0Count = 0;
            if (uv0CountVal)
            {
                JsonValue val;
                ParseJsonValue(uv0CountVal, val);
                uv0Count = (val.type == JsonType::Number) ? val.AsUint32() : 0;
            }
            uv0Data = ExtractAccessorData(uv0AccJson, uv0AccIdx, uv0Count, uv0CompType, 2);
        }
    }

    // Build the vertex array from extracted data.
    auto& lod = asset.lods[0];
    lod.screenDistanceThreshold = 0.0f;
    lod.vertexCountHint = vertexCount;
    lod.vertices.resize(vertexCount);

    Render::MeshVec3 boundsMin{ 1e30f, 1e30f, 1e30f };
    Render::MeshVec3 boundsMax{-1e30f,-1e30f,-1e30f };

    for (std::uint32_t i = 0; i < vertexCount; ++i)
    {
        auto& v = lod.vertices[i];

        if (!positions.empty() && static_cast<std::size_t>(i) * 3 + 2 < positions.size())
        {
            v.position = {positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]};
            boundsMin.x = std::min(boundsMin.x, v.position.x);
            boundsMin.y = std::min(boundsMin.y, v.position.y);
            boundsMin.z = std::min(boundsMin.z, v.position.z);
            boundsMax.x = std::max(boundsMax.x, v.position.x);
            boundsMax.y = std::max(boundsMax.y, v.position.y);
            boundsMax.z = std::max(boundsMax.z, v.position.z);
        }
        else
        {
            v.position = {0.0f, 0.0f, 0.0f};
        }

        if (!normals.empty() && static_cast<std::size_t>(i) * 3 + 2 < normals.size())
        {
            v.normal = {normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]};
        }
        else
        {
            v.normal = {0.0f, 1.0f, 0.0f};
        }

        if (!tangents.empty() && static_cast<std::size_t>(i) * 4 + 3 < tangents.size())
        {
            v.tangent    = {tangents[i * 4 + 0], tangents[i * 4 + 1], tangents[i * 4 + 2]};
            v.handedness = tangents[i * 4 + 3];
        }
        else
        {
            v.tangent    = {1.0f, 0.0f, 0.0f};
            v.handedness = 1.0f;
        }

        if (!uv0Data.empty() && static_cast<std::size_t>(i) * 2 + 1 < uv0Data.size())
        {
            v.uv0 = {uv0Data[i * 2 + 0], uv0Data[i * 2 + 1]};
        }
        else
        {
            v.uv0 = {0.0f, 0.0f};
        }

        v.uv1 = {0.0f, 0.0f};
    }

    // Parse index count and data from indices accessor if present.
    std::uint32_t indexCount = 0;
    if (indicesAccIdx != 0xFFFFFFFFu)
    {
        const char* idxAcc = FindArrayElement(accessorsVal, indicesAccIdx);
        if (idxAcc)
        {
            const char* idxCountVal = FindJsonKey(idxAcc, 256, "count");
            if (idxCountVal)
            {
                JsonValue val;
                ParseJsonValue(idxCountVal, val);
                indexCount = (val.type == JsonType::Number) ? val.AsUint32() : 0;
            }

            // Parse index data from the binary buffer.
            // Indices can be uint8, uint16, or uint32 depending on
            // componentType (5121, 5123, 5125).
            if (indexCount > 0 && indexCount <= Render::kMaxMeshIndicesPerLod && !binData.empty())
            {
                const char* idxBvVal = FindJsonKey(idxAcc, 256, "bufferView");
                if (idxBvVal)
                {
                    JsonValue bvIdxVal;
                    ParseJsonValue(idxBvVal, bvIdxVal);
                    if (bvIdxVal.type == JsonType::Number)
                    {
                        std::uint32_t idxBvIdx = bvIdxVal.AsUint32();
                        const char* idxBvJson = FindArrayElement(bufferViewsVal, idxBvIdx);
                        if (idxBvJson)
                        {
                            const char* idxOffVal = FindJsonKey(idxBvJson, 256, "byteOffset");
                            std::uint32_t idxOffset = 0;
                            if (idxOffVal)
                            {
                                JsonValue val;
                                ParseJsonValue(idxOffVal, val);
                                idxOffset = (val.type == JsonType::Number) ? val.AsUint32() : 0;
                            }

                            const char* idxCompVal = FindJsonKey(idxAcc, 256, "componentType");
                            std::uint32_t idxCompType = 5125; // Default uint32.
                            if (idxCompVal)
                            {
                                JsonValue val;
                                ParseJsonValue(idxCompVal, val);
                                idxCompType = (val.type == JsonType::Number) ? val.AsUint32() : 5125;
                            }

                            // Read indices based on component type.
                            lod.indexCountHint = indexCount;
                            lod.indices.resize(indexCount);
                            if (idxOffset + indexCount * 4 <= binData.size())
                            {
                                for (std::uint32_t i = 0; i < indexCount; ++i)
                                {
                                    if (idxCompType == 5121 && idxOffset + i + 1 <= binData.size())
                                    {
                                        // uint8 indices.
                                        lod.indices[i] = static_cast<std::uint32_t>(binData[idxOffset + i]);
                                    }
                                    else if (idxCompType == 5123 && idxOffset + (i + 1) * 2 <= binData.size())
                                    {
                                        // uint16 indices (little-endian).
                                        std::uint16_t idx16 = 0;
                                        std::memcpy(&idx16, binData.data() + idxOffset + i * 2, 2);
                                        lod.indices[i] = static_cast<std::uint32_t>(idx16);
                                    }
                                    else if (idxCompType == 5125 && idxOffset + (i + 1) * 4 <= binData.size())
                                    {
                                        // uint32 indices (little-endian).
                                        std::memcpy(&lod.indices[i], binData.data() + idxOffset + i * 4, 4);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Fallback: if no index data, use linear indices.
    if (lod.indices.empty() && vertexCount > 0)
    {
        lod.indexCountHint = vertexCount;
        lod.indices.resize(vertexCount);
        for (std::uint32_t i = 0; i < vertexCount; ++i)
            lod.indices[i] = i;
    }

    // Setup submesh.
    Render::MeshSubmesh submesh;
    submesh.indexOffset = 0;
    submesh.indexCount  = indexCount > 0 ? indexCount : vertexCount;
    submesh.materialId  = 0;
    submesh.debugName   = "submesh_0";
    lod.submeshes.push_back(std::move(submesh));

    // Compute bounding box from actual vertex data.
    if (positions.empty())
    {
        asset.rootBounds.centre      = {0.0f, 0.0f, 0.0f};
        asset.rootBounds.halfExtents = {1.0f, 1.0f, 1.0f};
    }
    else
    {
        asset.rootBounds.centre = {
            (boundsMin.x + boundsMax.x) * 0.5f,
            (boundsMin.y + boundsMax.y) * 0.5f,
            (boundsMin.z + boundsMax.z) * 0.5f
        };
        asset.rootBounds.halfExtents = {
            (boundsMax.x - boundsMin.x) * 0.5f,
            (boundsMax.y - boundsMin.y) * 0.5f,
            (boundsMax.z - boundsMin.z) * 0.5f
        };
    }

    // Compute approximate GPU size.
    lod.approxGpuBytes = static_cast<std::uint64_t>(lod.vertices.size()) * sizeof(Render::MeshVertex)
                       + static_cast<std::uint64_t>(lod.indices.size()) * sizeof(std::uint32_t);

    result.success = true;
    return result;
}

// ─── ImportFromBytes ───────────────────────────────────────────────────────

GltfImportResult GltfMeshImporter::ImportFromBytes(
    const std::vector<std::uint8_t>& bytes,
    const std::string&               sourcePath,
    std::uint64_t                    meshId)
{
    if (bytes.empty())
    {
        return {false, {}, "GltfMeshImporter: empty byte input"};
    }

    // Detect GLB by magic header.
    if (bytes.size() >= 4)
    {
        std::uint32_t magic = 0;
        std::memcpy(&magic, bytes.data(), 4);
        if (magic == kGlbMagic)
        {
            // GLB binary path.
            std::vector<std::uint8_t> jsonChunk, binChunk;
            if (!ExtractGlbChunks(bytes, jsonChunk, binChunk))
                return {false, {}, "GltfMeshImporter: failed to extract GLB chunks"};

            std::string jsonText(reinterpret_cast<const char*>(jsonChunk.data()));
            return ParseGltfJson(jsonText, binChunk, sourcePath, meshId);
        }
    }

    // Assume JSON text (.gltf).
    std::string jsonText(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::vector<std::uint8_t> emptyBin;
    return ParseGltfJson(jsonText, emptyBin, sourcePath, meshId);
}

// ─── ImportFromFile ────────────────────────────────────────────────────────

GltfImportResult GltfMeshImporter::ImportFromFile(
    const std::string& filePath,
    std::uint64_t      meshId)
{
    std::FILE* file = std::fopen(filePath.c_str(), "rb");
    if (!file)
        return {false, {}, "GltfMeshImporter: cannot open file '" + filePath + "'"};

    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::rewind(file);

    if (fileSize <= 0)
    {
        std::fclose(file);
        return {false, {}, "GltfMeshImporter: empty file '" + filePath + "'"};
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    if (std::fread(bytes.data(), 1, static_cast<std::size_t>(fileSize), file) != static_cast<std::size_t>(fileSize))
    {
        std::fclose(file);
        return {false, {}, "GltfMeshImporter: failed to read '" + filePath + "'"};
    }
    std::fclose(file);

    return ImportFromBytes(bytes, filePath, meshId);
}

} // namespace SagaEngine::Resources
