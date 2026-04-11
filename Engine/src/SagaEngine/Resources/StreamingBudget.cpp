/// @file StreamingBudget.cpp
/// @brief Per-platform memory budget configuration for asset streaming.

#include "SagaEngine/Resources/StreamingBudget.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>

namespace SagaEngine::Resources {

namespace {

constexpr const char* kLogTag = "Resources";

} // namespace

// ─── Default profiles ──────────────────────────────────────────────────────

StreamingBudgetConfig StreamingBudget::DefaultProfile(
    StreamingPlatformProfile profile) noexcept
{
    StreamingBudgetConfig config;

    switch (profile)
    {
        case StreamingPlatformProfile::DesktopHighEnd:
            config.profileName               = "Desktop High-End";
            config.rawStreamingSoftLimit     = 1024ull * 1024 * 1024;  // 1 GiB.
            config.rawStreamingHardLimit     = 2048ull * 1024 * 1024;  // 2 GiB.
            config.decodedMeshSoftLimit      = 512ull * 1024 * 1024;   // 512 MiB.
            config.decodedMeshHardLimit      = 1024ull * 1024 * 1024;  // 1 GiB.
            config.decodedTextureSoftLimit   = 1024ull * 1024 * 1024;  // 1 GiB.
            config.decodedTextureHardLimit   = 2048ull * 1024 * 1024;  // 2 GiB.
            config.maxConcurrentPrefetch     = 64;
            config.lodDistanceMultiplier     = 1.0f;
            break;

        case StreamingPlatformProfile::DesktopMidRange:
            config.profileName               = "Desktop Mid-Range";
            config.rawStreamingSoftLimit     = 512ull * 1024 * 1024;   // 512 MiB.
            config.rawStreamingHardLimit     = 768ull * 1024 * 1024;   // 768 MiB.
            config.decodedMeshSoftLimit      = 128ull * 1024 * 1024;   // 128 MiB.
            config.decodedMeshHardLimit      = 256ull * 1024 * 1024;   // 256 MiB.
            config.decodedTextureSoftLimit   = 256ull * 1024 * 1024;   // 256 MiB.
            config.decodedTextureHardLimit   = 512ull * 1024 * 1024;   // 512 MiB.
            config.maxConcurrentPrefetch     = 32;
            config.lodDistanceMultiplier     = 1.2f; // Prefer coarser LODs sooner.
            break;

        case StreamingPlatformProfile::ConsoleNextGen:
            config.profileName               = "Console Next-Gen";
            config.rawStreamingSoftLimit     = 2048ull * 1024 * 1024;  // 2 GiB.
            config.rawStreamingHardLimit     = 3072ull * 1024 * 1024;  // 3 GiB.
            config.decodedMeshSoftLimit      = 512ull * 1024 * 1024;   // 512 MiB.
            config.decodedMeshHardLimit      = 1024ull * 1024 * 1024;  // 1 GiB.
            config.decodedTextureSoftLimit   = 1024ull * 1024 * 1024;  // 1 GiB.
            config.decodedTextureHardLimit   = 2048ull * 1024 * 1024;  // 2 GiB.
            config.maxConcurrentPrefetch     = 48;
            config.lodDistanceMultiplier     = 0.9f; // Can afford finer LODs.
            break;

        case StreamingPlatformProfile::MobileHigh:
            config.profileName               = "Mobile High";
            config.rawStreamingSoftLimit     = 256ull * 1024 * 1024;   // 256 MiB.
            config.rawStreamingHardLimit     = 384ull * 1024 * 1024;   // 384 MiB.
            config.decodedMeshSoftLimit      = 64ull * 1024 * 1024;    // 64 MiB.
            config.decodedMeshHardLimit      = 128ull * 1024 * 1024;   // 128 MiB.
            config.decodedTextureSoftLimit   = 128ull * 1024 * 1024;   // 128 MiB.
            config.decodedTextureHardLimit   = 256ull * 1024 * 1024;   // 256 MiB.
            config.maxConcurrentPrefetch     = 16;
            config.lodDistanceMultiplier     = 1.5f; // Aggressively use coarse LODs.
            break;

        case StreamingPlatformProfile::MobileLow:
            config.profileName               = "Mobile Low";
            config.rawStreamingSoftLimit     = 128ull * 1024 * 1024;   // 128 MiB.
            config.rawStreamingHardLimit     = 192ull * 1024 * 1024;   // 192 MiB.
            config.decodedMeshSoftLimit      = 32ull * 1024 * 1024;    // 32 MiB.
            config.decodedMeshHardLimit      = 64ull * 1024 * 1024;    // 64 MiB.
            config.decodedTextureSoftLimit   = 64ull * 1024 * 1024;    // 64 MiB.
            config.decodedTextureHardLimit   = 128ull * 1024 * 1024;   // 128 MiB.
            config.maxConcurrentPrefetch     = 8;
            config.lodDistanceMultiplier     = 2.0f; // Very aggressive LOD reduction.
            break;

        default:
            config.profileName = "Unknown";
            break;
    }

    return config;
}

// ─── Configure ─────────────────────────────────────────────────────────────

void StreamingBudget::Configure(StreamingBudgetConfig config) noexcept
{
    config_ = std::move(config);

    LOG_INFO(kLogTag,
             "StreamingBudget: configured profile='%s' "
             "rawSoft=%llu rawHard=%llu meshSoft=%llu meshHard=%llu texSoft=%llu texHard=%llu",
             config_.profileName.c_str(),
             static_cast<unsigned long long>(config_.rawStreamingSoftLimit),
             static_cast<unsigned long long>(config_.rawStreamingHardLimit),
             static_cast<unsigned long long>(config_.decodedMeshSoftLimit),
             static_cast<unsigned long long>(config_.decodedMeshHardLimit),
             static_cast<unsigned long long>(config_.decodedTextureSoftLimit),
             static_cast<unsigned long long>(config_.decodedTextureHardLimit));
}

// ─── UpdateResidency ───────────────────────────────────────────────────────

void StreamingBudget::UpdateResidency(std::uint64_t rawStreamingBytes,
                                      std::uint64_t decodedMeshBytes,
                                      std::uint64_t decodedTextureBytes) noexcept
{
    currentRawStreaming_   = rawStreamingBytes;
    currentDecodedMesh_    = decodedMeshBytes;
    currentDecodedTexture_ = decodedTextureBytes;
}

// ─── Pressure ──────────────────────────────────────────────────────────────

float StreamingBudget::Pressure() const noexcept
{
    // Compute the pressure ratio for each subsystem independently,
    /// then return the maximum — the most constrained subsystem
    /// dictates overall streaming behaviour.
    auto Ratio = [](std::uint64_t current, std::uint64_t soft, std::uint64_t hard) -> float {
        if (hard == 0)
            return 0.0f;
        if (current >= hard)
            return 1.0f;
        if (current <= soft)
            return 0.0f;
        // Linear interpolation between soft and hard.
        return static_cast<float>(current - soft) / static_cast<float>(hard - soft);
    };

    float rawPressure   = Ratio(currentRawStreaming_,  config_.rawStreamingSoftLimit,   config_.rawStreamingHardLimit);
    float meshPressure  = Ratio(currentDecodedMesh_,   config_.decodedMeshSoftLimit,    config_.decodedMeshHardLimit);
    float texPressure   = Ratio(currentDecodedTexture_, config_.decodedTextureSoftLimit, config_.decodedTextureHardLimit);

    return std::max({rawPressure, meshPressure, texPressure});
}

// ─── WouldExceedHardLimit ──────────────────────────────────────────────────

bool StreamingBudget::WouldExceedHardLimit(std::uint64_t additionalBytes,
                                           const std::string& subsystem) const noexcept
{
    if (subsystem == "raw")
        return (currentRawStreaming_ + additionalBytes) > config_.rawStreamingHardLimit;
    if (subsystem == "mesh")
        return (currentDecodedMesh_ + additionalBytes) > config_.decodedMeshHardLimit;
    if (subsystem == "texture")
        return (currentDecodedTexture_ + additionalBytes) > config_.decodedTextureHardLimit;

    // Unknown subsystem — fail safe.
    return true;
}

} // namespace SagaEngine::Resources
