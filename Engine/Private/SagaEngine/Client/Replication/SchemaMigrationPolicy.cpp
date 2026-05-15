/// @file SchemaMigrationPolicy.cpp
/// @file Schema version negotiation and backward compatibility policy.

#include "SagaEngine/Client/Replication/SchemaMigrationPolicy.h"

#include <algorithm>
#include <cstdio>

namespace SagaEngine::Client::Replication {

namespace {

inline constexpr std::uint32_t kMajorShift  = 22;  // bits 22-31
inline constexpr std::uint32_t kMinorShift  = 12;  // bits 12-21
inline constexpr std::uint32_t kMajorMask   = 0xFFC00000u;
inline constexpr std::uint32_t kMinorMask   = 0x003FF000u;
inline constexpr std::uint32_t kPatchMask   = 0x00000FFFu;

} // namespace

// ─── Pack / unpack ──────────────────────────────────────────────────────────

std::uint32_t SchemaVersion::Pack() const noexcept
{
    return (static_cast<std::uint32_t>(major) << kMajorShift)
         | (static_cast<std::uint32_t>(minor) << kMinorShift)
         | (static_cast<std::uint32_t>(patch) & kPatchMask);
}

SchemaVersion UnpackVersion(std::uint32_t packed) noexcept
{
    SchemaVersion v;
    v.major = static_cast<std::uint16_t>((packed & kMajorMask) >> kMajorShift);
    v.minor = static_cast<std::uint16_t>((packed & kMinorMask) >> kMinorShift);
    v.patch = static_cast<std::uint16_t>(packed & kPatchMask);
    return v;
}

std::string SchemaVersion::Format() const noexcept
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u",
                  static_cast<unsigned>(major),
                  static_cast<unsigned>(minor),
                  static_cast<unsigned>(patch));
    return std::string(buf);
}

// ─── Policy ─────────────────────────────────────────────────────────────────

void SchemaMigrationPolicy::SetClientVersion(SchemaVersion minSupported,
                                              SchemaVersion current) noexcept
{
    minSupported_ = minSupported;
    current_ = current;
}

CompatibilityResult SchemaMigrationPolicy::CheckCompatibility(
    SchemaVersion serverVersion,
    SchemaVersion* outNegotiatedVersion) noexcept
{
    // Major mismatch = incompatible.
    if (serverVersion.major != current_.major)
    {
        if (outNegotiatedVersion)
            *outNegotiatedVersion = current_;
        return CompatibilityResult::Incompatible;
    }

    // Negotiate: use the minor version the client supports.
    SchemaVersion negotiated;
    negotiated.major = serverVersion.major;
    negotiated.minor = std::min(serverVersion.minor, current_.minor);
    negotiated.patch = serverVersion.patch;  // Server's patch is authoritative.

    if (outNegotiatedVersion)
        *outNegotiatedVersion = negotiated;

    negotiatedVersion_ = negotiated;

    if (serverVersion.minor > current_.minor)
        return CompatibilityResult::ForwardCompatible;

    if (serverVersion.minor < current_.minor)
        return CompatibilityResult::BackwardCompatible;

    return CompatibilityResult::Compatible;
}

bool SchemaMigrationPolicy::ShouldSkipComponent(std::uint16_t serverMinor,
                                                  std::uint16_t clientMinor) const noexcept
{
    // If the server's minor is newer than what the client supports,
    // the client should skip component types it does not recognise.
    return serverMinor > clientMinor;
}

} // namespace SagaEngine::Client::Replication
