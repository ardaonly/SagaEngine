/// @file StreamingBudget.h
/// @brief Per-platform memory budget configuration for asset streaming.
///
/// Layer  : SagaEngine / Resources
/// Purpose: An MMO client runs on very different hardware — a desktop
///          gaming PC may have 32 GiB of RAM while a mobile device
///          struggles with 4 GiB.  The streaming subsystem must
///          respect these constraints: the total bytes consumed by
///          resident meshes, textures, and audio must not exceed the
///          platform's budget, and when memory pressure rises the
///          streaming manager evicts the least-recently-used assets
///          to stay within bounds.
///
///          `StreamingBudget` is the configuration layer that defines
///          those ceilings.  It ships with sensible defaults for each
///          target platform (Windows desktop, console, mobile) and
///          allows runtime tuning so the editor can simulate low-end
///          hardware on a beefy development machine.
///
/// Design rules:
///   - Three independent budgets.  Meshes, textures, and decoded
///     assets each get their own ceiling so a zone full of 4K
///     textures cannot starve the mesh cache (and vice versa).
///   - Soft and hard limits.  The soft limit triggers LRU eviction;
///     the hard limit rejects new acquisitions outright.  This gives
///     the streaming manager a grace period to shed load gracefully
///     before it starts failing requests.
///   - Observable.  The budget exposes a `Pressure` ratio so the
///     zone loader can throttle prefetch intensity when memory is
///     tight.
///
/// What this header is NOT:
///   - Not a garbage collector.  Eviction is LRU-based and
///     deterministic — there is no background sweeper.
///   - Not a platform detector.  The caller supplies the profile;
///     this file only stores the numbers.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEngine::Resources {

// ─── Platform profile ──────────────────────────────────────────────────────

/// Predefined target platforms.  Each profile carries a set of
/// budgets tuned for the typical hardware of that class.
enum class StreamingPlatformProfile : std::uint8_t
{
    Unknown = 0,

    /// Desktop PC (8–32 GiB RAM).  Generous budgets, aggressive
    /// prefetch, high LOD distances.
    DesktopHighEnd,

    /// Desktop PC (4–8 GiB RAM).  Moderate budgets, conservative
    /// prefetch.
    DesktopMidRange,

    /// Console (PS5 / Xbox Series X).  Large unified memory, but
    /// the OS reserves a significant portion — budgets are tuned
    /// for the ~10 GiB available to games.
    ConsoleNextGen,

    /// Mobile / tablet (4–6 GiB RAM).  Tight budgets, minimal
    /// prefetch, low LOD distances.
    MobileHigh,

    /// Low-end mobile (2–3 GiB RAM).  Very tight budgets,
    /// deferred streaming (only load what is immediately needed).
    MobileLow,
};

// ─── Budget configuration ──────────────────────────────────────────────────

/// Per-subsystem memory budget.  All values are in bytes.
struct StreamingBudgetConfig
{
    // ─── Raw streaming budget (IAssetSource → StreamingManager) ────────

    /// Soft ceiling for raw streamed bytes.  The streaming manager
    /// begins LRU eviction when residency exceeds this value.
    std::uint64_t rawStreamingSoftLimit = 512ull * 1024 * 1024; // 512 MiB.

    /// Hard ceiling for raw streamed bytes.  New acquisitions are
    /// rejected once residency exceeds this value.
    std::uint64_t rawStreamingHardLimit = 768ull * 1024 * 1024; // 768 MiB.

    // ─── Decoded mesh cache budget ─────────────────────────────────────

    /// Soft ceiling for decoded mesh data (vertex + index buffers
    /// on the CPU side — GPU buffers are tracked separately by the
    /// RHI).
    std::uint64_t decodedMeshSoftLimit = 128ull * 1024 * 1024; // 128 MiB.

    /// Hard ceiling for decoded mesh data.
    std::uint64_t decodedMeshHardLimit = 256ull * 1024 * 1024; // 256 MiB.

    // ─── Decoded texture cache budget ──────────────────────────────────

    /// Soft ceiling for decoded texture mip data.  GPU-resident
    /// textures are tracked by the RHI; this budget covers the CPU
    /// side cache that feeds uploads.
    std::uint64_t decodedTextureSoftLimit = 256ull * 1024 * 1024; // 256 MiB.

    /// Hard ceiling for decoded texture data.
    std::uint64_t decodedTextureHardLimit = 512ull * 1024 * 1024; // 512 MiB.

    // ─── Prefetch intensity ────────────────────────────────────────────

    /// Maximum number of concurrent prefetch requests.  When the
    /// zone loader seeds the streaming queue with apron assets, it
    /// caps the in-flight count at this value to avoid saturating
    /// the I/O subsystem.
    std::uint32_t maxConcurrentPrefetch = 32;

    /// LOD distance multiplier.  On low-end platforms this value
    /// is reduced so coarse LODs are preferred at shorter distances
    /// (saving GPU memory and bandwidth).
    float lodDistanceMultiplier = 1.0f;

    // ─── Metadata ──────────────────────────────────────────────────────

    /// Human-readable label for logs and diagnostics.
    std::string profileName = "custom";
};

// ─── StreamingBudget ───────────────────────────────────────────────────────

/// Per-platform budget configuration.  Provides default profiles
/// and runtime tuning.
class StreamingBudget
{
public:
    StreamingBudget() noexcept  = default;
    ~StreamingBudget() noexcept = default;

    // ─── Default profiles ────────────────────────────────────────────────

    /// Load a predefined platform profile.  The caller can then
    /// tweak individual values before handing the config to the
    /// streaming manager.
    [[nodiscard]] static StreamingBudgetConfig DefaultProfile(
        StreamingPlatformProfile profile) noexcept;

    // ─── Current config access ───────────────────────────────────────────

    /// Set the active budget configuration.  Used at boot and when
    /// the operator changes the profile at runtime (e.g. the user
    /// switches from "High" to "Low" graphics settings).
    void Configure(StreamingBudgetConfig config) noexcept;

    /// Get the current configuration.  Callers read this to decide
    /// prefetch intensity, LOD distances, and eviction thresholds.
    [[nodiscard]] const StreamingBudgetConfig& Config() const noexcept { return config_; }

    // ─── Pressure tracking ───────────────────────────────────────────────

    /// Update the current residency counters.  Called by the
    /// streaming manager every time a request completes or an
    /// asset is evicted.
    void UpdateResidency(std::uint64_t rawStreamingBytes,
                         std::uint64_t decodedMeshBytes,
                         std::uint64_t decodedTextureBytes) noexcept;

    /// Memory pressure ratio in [0, 1].  Zero means all subsystems
    /// are well under their soft limits; one means at least one
    /// subsystem has hit its hard limit.  The zone loader uses this
    /// to throttle prefetch: when pressure is high, it only streams
    /// critical assets.
    [[nodiscard]] float Pressure() const noexcept;

    /// Test whether a new acquisition of the given size would exceed
    /// the hard limit for the specified subsystem.  Used by the
    /// streaming manager to reject requests before they allocate.
    [[nodiscard]] bool WouldExceedHardLimit(std::uint64_t additionalBytes,
                                            const std::string& subsystem) const noexcept;

private:
    StreamingBudgetConfig config_;

    // Current residency counters — updated by the streaming manager.
    std::uint64_t currentRawStreaming_    = 0;
    std::uint64_t currentDecodedMesh_     = 0;
    std::uint64_t currentDecodedTexture_  = 0;
};

} // namespace SagaEngine::Resources
