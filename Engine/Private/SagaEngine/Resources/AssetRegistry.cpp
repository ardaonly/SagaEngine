/// @file AssetRegistry.cpp
/// @brief Content manifest — JSON parsing and lookup implementation.

#include "SagaEngine/Resources/AssetRegistry.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace SagaEngine::Resources {

namespace {

constexpr const char* kLogTag = "Resources";

// ─── Minimal JSON parser ───────────────────────────────────────────────────

/// The manifest format is intentionally simple — a JSON array of
/// objects.  Rather than pulling in a third-party JSON library (which
/// would add a transitive dependency to the resources subsystem) we
/// implement a minimal hand-rolled parser that understands exactly
/// the fields we need.  This is not a general-purpose JSON parser:
/// it does not support nested objects, arrays, or string escaping
/// beyond basic quotes.  It is good enough for the manifest format
/// and nothing more.

/// Skip whitespace.  Advances `p` past spaces, tabs, newlines.
[[nodiscard]] const char* SkipWs(const char* p) noexcept
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        ++p;
    return p;
}

/// Expect a character.  Returns nullptr on mismatch.
[[nodiscard]] const char* ExpectChar(const char* p, char c, const char*& outError) noexcept
{
    p = SkipWs(p);
    if (*p != c)
    {
        outError = "expected character";
        return nullptr;
    }
    return p + 1;
}

/// Parse a JSON string (no escape handling beyond backslash-quote).
/// Returns nullptr on error.
[[nodiscard]] const char* ParseString(const char* p, std::string& out, const char*& outError) noexcept
{
    p = SkipWs(p);
    if (*p != '"')
    {
        outError = "expected '\"'";
        return nullptr;
    }
    ++p;

    out.clear();
    while (*p && *p != '"')
    {
        if (*p == '\\' && *(p + 1) == '"')
        {
            out += '"';
            p += 2;
        }
        else
        {
            out += *p;
            ++p;
        }
    }

    if (*p != '"')
    {
        outError = "unterminated string";
        return nullptr;
    }
    return p + 1;
}

/// Parse a JSON number (unsigned 64-bit).  Returns nullptr on error.
[[nodiscard]] const char* ParseUint64(const char* p, std::uint64_t& out, const char*& outError) noexcept
{
    p = SkipWs(p);
    if (*p < '0' || *p > '9')
    {
        outError = "expected digit";
        return nullptr;
    }

    out = 0;
    while (*p >= '0' && *p <= '9')
    {
        out = out * 10 + static_cast<std::uint64_t>(*p - '0');
        ++p;
    }
    return p;
}

/// Parse a JSON number (unsigned 32-bit).  Returns nullptr on error.
[[nodiscard]] const char* ParseUint32(const char* p, std::uint32_t& out, const char*& outError) noexcept
{
    std::uint64_t val = 0;
    p = ParseUint64(p, val, outError);
    if (!p)
        return nullptr;
    out = static_cast<std::uint32_t>(val);
    return p;
}

/// Map a string to AssetKind.  Returns Unknown on unknown string.
[[nodiscard]] AssetKind StringToKind(const std::string& s) noexcept
{
    if (s == "Mesh")       return AssetKind::Mesh;
    if (s == "Texture")    return AssetKind::Texture;
    if (s == "Shader")     return AssetKind::Shader;
    if (s == "Audio")      return AssetKind::Audio;
    if (s == "Animation")  return AssetKind::Animation;
    if (s == "Material")   return AssetKind::MaterialAsset;
    return AssetKind::Unknown;
}

/// Parse one registry entry object.  Assumes `p` points just after
/// the opening '{'.  Returns nullptr on error.
[[nodiscard]] const char* ParseEntry(const char* p, AssetRegistryEntry& entry, const char*& outError) noexcept
{
    while (true)
    {
        p = SkipWs(p);
        if (*p == '}')
            return p + 1;
        if (*p == ',')
        {
            ++p;
            continue;
        }
        if (!*p)
        {
            outError = "unterminated object";
            return nullptr;
        }

        // Parse key.
        std::string key;
        p = ParseString(p, key, outError);
        if (!p)
            return nullptr;

        p = ExpectChar(p, ':', outError);
        if (!p)
            return nullptr;

        // Parse value based on key.
        if (key == "id")
        {
            std::uint64_t val = 0;
            p = ParseUint64(p, val, outError);
            if (!p) return nullptr;
            entry.assetId = val;
        }
        else if (key == "kind")
        {
            std::string val;
            p = ParseString(p, val, outError);
            if (!p) return nullptr;
            entry.kind = StringToKind(val);
        }
        else if (key == "path")
        {
            p = ParseString(p, entry.sourcePath, outError);
            if (!p) return nullptr;
        }
        else if (key == "flags")
        {
            p = ParseUint32(p, entry.flags, outError);
            if (!p) return nullptr;
        }
        else if (key == "sizeBytes")
        {
            p = ParseUint64(p, entry.diskSizeBytes, outError);
            if (!p) return nullptr;
        }
        else if (key == "lodCount")
        {
            std::uint32_t val = 0;
            p = ParseUint32(p, val, outError);
            if (!p) return nullptr;
            entry.lodCount = static_cast<std::uint8_t>(val);
        }
        else
        {
            // Unknown key — skip its value.
            // Simple skip: if value is a string, skip it; if number, skip digits.
            p = SkipWs(p);
            if (*p == '"')
            {
                std::string dummy;
                p = ParseString(p, dummy, outError);
                if (!p) return nullptr;
            }
            else
            {
                while (*p >= '0' && *p <= '9')
                    ++p;
            }
        }
    }
}

} // namespace

// ─── LoadFromJson ──────────────────────────────────────────────────────────

bool AssetRegistry::LoadFromJson(const std::string& manifestPath)
{
    // Read the entire file into memory.  Manifests are typically a few
    /// hundred kilobytes even for large games, so a single read is fine.
    std::FILE* file = std::fopen(manifestPath.c_str(), "rb");
    if (!file)
    {
        LOG_ERROR(kLogTag,
                  "AssetRegistry: cannot open manifest '%s'",
                  manifestPath.c_str());
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::rewind(file);

    if (fileSize <= 0)
    {
        LOG_ERROR(kLogTag,
                  "AssetRegistry: empty manifest '%s'",
                  manifestPath.c_str());
        std::fclose(file);
        return false;
    }

    std::string buffer(static_cast<std::size_t>(fileSize), '\0');
    if (std::fread(buffer.data(), 1, static_cast<std::size_t>(fileSize), file) != static_cast<std::size_t>(fileSize))
    {
        LOG_ERROR(kLogTag,
                  "AssetRegistry: failed to read manifest '%s'",
                  manifestPath.c_str());
        std::fclose(file);
        return false;
    }
    std::fclose(file);

    // Null-terminate for safe pointer arithmetic.
    buffer.push_back('\0');

    // Parse.
    const char* p         = buffer.c_str();
    const char* outError  = nullptr;

    p = ExpectChar(p, '[', outError);
    if (!p)
    {
        LOG_ERROR(kLogTag,
                  "AssetRegistry: manifest parse error: %s (expected '[')",
                  outError);
        return false;
    }

    Clear();

    std::size_t entryCount = 0;

    while (true)
    {
        p = SkipWs(p);
        if (*p == ']')
            break;
        if (*p == ',')
        {
            ++p;
            continue;
        }
        if (*p == '{')
        {
            ++p;
            AssetRegistryEntry entry;
            p = ParseEntry(p, entry, outError);
            if (!p)
            {
                LOG_ERROR(kLogTag,
                          "AssetRegistry: manifest parse error at entry %zu: %s",
                          entryCount, outError);
                return false;
            }

            if (entry.assetId == kInvalidAssetId)
            {
                LOG_WARN(kLogTag,
                         "AssetRegistry: skipping entry %zu with invalid id (path='%s')",
                         entryCount, entry.sourcePath.c_str());
                continue;
            }

            Insert(std::move(entry));
            ++entryCount;
        }
        else
        {
            LOG_ERROR(kLogTag,
                      "AssetRegistry: manifest parse error: unexpected character '%c'",
                      *p);
            return false;
        }
    }

    LOG_INFO(kLogTag,
             "AssetRegistry: loaded %zu entries from '%s'",
             entryCount, manifestPath.c_str());
    return true;
}

// ─── Insert ────────────────────────────────────────────────────────────────

void AssetRegistry::Insert(AssetRegistryEntry entry)
{
    auto it = entriesBy_.find(entry.assetId);
    if (it != entriesBy_.end())
    {
        // Overwrite existing entry.  This is intentional — the
        // caller may be reloading the manifest or patching an
        // entry at runtime.
        *it->second = std::move(entry);
    }
    else
    {
        auto ptr    = std::make_unique<AssetRegistryEntry>(std::move(entry));
        auto* raw   = ptr.get();
        entriesBy_[raw->assetId] = std::move(ptr);
    }
}

// ─── Clear ─────────────────────────────────────────────────────────────────

void AssetRegistry::Clear() noexcept
{
    entriesBy_.clear();
}

// ─── Find ──────────────────────────────────────────────────────────────────

const AssetRegistryEntry* AssetRegistry::Find(AssetId assetId) const noexcept
{
    auto it = entriesBy_.find(assetId);
    return (it != entriesBy_.end()) ? it->second.get() : nullptr;
}

// ─── FindByKind ────────────────────────────────────────────────────────────

std::vector<const AssetRegistryEntry*> AssetRegistry::FindByKind(AssetKind kind) const
{
    std::vector<const AssetRegistryEntry*> result;
    result.reserve(entriesBy_.size() / 4); // Heuristic — one quarter per kind.

    for (const auto& [id, entry] : entriesBy_)
    {
        (void)id;
        if (entry->kind == kind)
            result.push_back(entry.get());
    }

    return result;
}

// ─── GetPrefetchCandidates ─────────────────────────────────────────────────

std::vector<const AssetRegistryEntry*> AssetRegistry::GetPrefetchCandidates() const
{
    std::vector<const AssetRegistryEntry*> result;

    for (const auto& [id, entry] : entriesBy_)
    {
        (void)id;
        if (entry->flags & AssetFlags::kPrefetch)
            result.push_back(entry.get());
    }

    // Sort by asset id so the streaming queue gets a deterministic
    // order — useful for reproducible load times in profiling.
    std::sort(result.begin(), result.end(),
              [](const AssetRegistryEntry* a, const AssetRegistryEntry* b) {
                  return a->assetId < b->assetId;
              });

    return result;
}

// ─── TotalDiskSizeBytes ────────────────────────────────────────────────────

std::uint64_t AssetRegistry::TotalDiskSizeBytes() const noexcept
{
    std::uint64_t total = 0;
    for (const auto& [id, entry] : entriesBy_)
    {
        (void)id;
        if (entry->diskSizeBytes > 0)
            total += entry->diskSizeBytes;
    }
    return total;
}

} // namespace SagaEngine::Resources
