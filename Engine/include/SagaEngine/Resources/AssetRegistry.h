/// @file AssetRegistry.h
/// @brief Content manifest — stable AssetId ↔ source-path lookup.
///
/// Layer  : SagaEngine / Resources
/// Purpose: In a shipped MMO the asset library is too large to keep
///          in memory, and paths on disk are meaningless to the
///          runtime (a mesh might be "hero_01.glb" on disk but the
///          server refers to it by id 10485761).  The registry is
///          the translation layer: it maps stable numeric ids to
///          source paths, asset kinds, LOD availability, and
///          residency hints so the streaming manager can fulfil a
///          request without ever touching a file glob.
///
///          The manifest is generated at cook time by the content
///          pipeline and loaded once on boot.  During development
///          the registry can be populated from a JSON file; in a
///          shipped build it may come from a signed binary blob or
///          a CDN manifest.
///
/// Design rules:
///   - O(1) lookup by asset id.  The hot path is a streaming worker
///     thread asking "what path does asset 12345 live at?" — a hash
///     table keeps that under a microsecond.
///   - Immutable after load.  The registry is built during boot and
///     never mutated at runtime; no mutex is needed for lookups,
///     which keeps the streaming workers lock-free on this surface.
///   - Lightweight.  Each entry stores only what the streaming
///     system needs to locate and budget the asset — editor
///     metadata, thumbnail paths, and dependency graphs live in
///     separate subsystems.
///
/// What this header is NOT:
///   - Not a dependency tracker.  If a material references a
///     texture, that graph is tracked by the renderer's material
///     system, not here.
///   - Not a virtual filesystem.  Paths are still paths — this
///     registry does not know or care whether the bytes come from
///     loose files, a package archive, or a network proxy.

#pragma once

#include "SagaEngine/Resources/StreamingRequest.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Resources {

// ─── Registry entry ────────────────────────────────────────────────────────

/// One asset in the content manifest.  Kept minimal — the registry
/// is a lookup table, not an asset database.
struct AssetRegistryEntry
{
    AssetId         assetId    = kInvalidAssetId;
    AssetKind       kind       = AssetKind::Unknown;
    std::string     sourcePath;        ///< Relative to the asset root.
    std::uint32_t   flags      = 0;    ///< Bitmask — see `AssetFlags`.

    /// Approximate byte size on disk.  Used by the streaming budget
    /// tracker to estimate memory pressure before the asset is
    /// loaded.  Zero means "unknown" — the streaming manager will
    /// measure the actual payload at load time.
    std::uint64_t   diskSizeBytes = 0;

    /// Number of LODs available.  Zero means the asset has no LODs
    /// (common for audio or shader assets); for meshes this is
    /// typically 1..4.
    std::uint8_t    lodCount   = 1;
};

// ─── Asset flags ───────────────────────────────────────────────────────────

namespace AssetFlags {

/// Asset is critical — must be resident before the zone loads.
inline constexpr std::uint32_t kCritical = 1u << 0;

/// Asset is a dependency of the current zone's streaming apron.
inline constexpr std::uint32_t kApronDependency = 1u << 1;

/// Asset has been cooked (optimised for runtime).  Uncooked assets
/// may require on-the-fly conversion at load time.
inline constexpr std::uint32_t kCooked = 1u << 2;

/// Asset is part of the editor UI — excluded from shipped builds.
inline constexpr std::uint32_t kEditorOnly = 1u << 3;

/// Asset should be prefetched on zone enter.  The streaming manager
/// uses this flag to seed the priority queue when a player crosses
/// a zone boundary.
inline constexpr std::uint32_t kPrefetch = 1u << 4;

} // namespace AssetFlags

// ─── Manifest file format (JSON) ───────────────────────────────────────────

/// The manifest file on disk is a JSON array of objects:
///
/// [
///   {
///     "id": 10485761,
///     "kind": "Mesh",
///     "path": "meshes/hero_01.glb",
///     "flags": 5,
///     "sizeBytes": 2458624,
///     "lodCount": 3
///   },
///   ...
/// ]
///
/// The parser lives in `AssetRegistry.cpp` — this header only
/// defines the in-memory representation.

// ─── AssetRegistry ─────────────────────────────────────────────────────────

/// Content manifest.  Built at boot from a manifest file or by
/// scanning a directory (development only); queried by the
/// streaming system throughout the process lifetime.
class AssetRegistry
{
public:
    AssetRegistry() noexcept  = default;
    ~AssetRegistry() noexcept = default;

    AssetRegistry(const AssetRegistry&)            = delete;
    AssetRegistry& operator=(const AssetRegistry&) = delete;
    AssetRegistry(AssetRegistry&&)                 = default;
    AssetRegistry& operator=(AssetRegistry&&)      = default;

    // ── Population ────────────────────────────────────────────────────────

    /// Load the registry from a JSON manifest file.  Replaces any
    /// existing entries — the caller is expected to call this once
    /// at boot.  Returns false on parse error or file-not-found.
    [[nodiscard]] bool LoadFromJson(const std::string& manifestPath);

    /// Insert one entry manually.  Used by tests and by the editor
    /// when the developer clicks "add asset to registry".  If an
    /// entry with the same id already exists, it is overwritten.
    void Insert(AssetRegistryEntry entry);

    /// Clear all entries.  Used at shutdown and by tests.
    void Clear() noexcept;

    // ── Lookup ────────────────────────────────────────────────────────────

    /// Look up an asset by id.  Returns nullptr if the id is not
    /// present — the pointer is valid for the lifetime of the
    /// registry (which is immovable after population).
    [[nodiscard]] const AssetRegistryEntry* Find(AssetId assetId) const noexcept;

    /// Look up all assets of a given kind.  The returned slice
    /// points into the registry's internal storage and is valid
    /// for the registry's lifetime.
    [[nodiscard]] std::vector<const AssetRegistryEntry*> FindByKind(AssetKind kind) const;

    /// Return all entries flagged for prefetch.  Used by the zone
    /// loader to seed the streaming queue when a player crosses a
    /// boundary.
    [[nodiscard]] std::vector<const AssetRegistryEntry*> GetPrefetchCandidates() const;

    // ── Diagnostics ───────────────────────────────────────────────────────

    /// Number of entries in the registry.
    [[nodiscard]] std::size_t EntryCount() const noexcept { return entriesBy_.size(); }

    /// Total disk footprint of all registered assets.  Zero entries
    /// whose `diskSizeBytes` is unknown are excluded from the sum.
    [[nodiscard]] std::uint64_t TotalDiskSizeBytes() const noexcept;

private:
    /// Primary lookup table — id → entry.  Stored as `unique_ptr`
    /// so the `Find()` pointer contract holds across rehashes.
    std::unordered_map<AssetId, std::unique_ptr<AssetRegistryEntry>> entriesBy_;
};

} // namespace SagaEngine::Resources
