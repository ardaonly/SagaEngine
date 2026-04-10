/// @file SchemaVersion.h
/// @brief Versioning metadata for snapshot / RPC wire schemas.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: An MMO's wire format is not stable — components get added,
///          fields get renamed, enums get extended.  We need three things
///          at once:
///
///            1. Detect when a peer speaks an incompatible version and
///               reject it *before* deserialising corrupt garbage.
///            2. Allow forward-compatible reads where possible — a 0.0.7
///               client should still be able to consume 0.0.6 snapshots.
///            3. Record enough metadata in logs / crash dumps to diagnose
///               schema drift after the fact.
///
///          This header just defines the *schema version tuple* and the
///          constants that every serialiser imprints on its frames.  The
///          actual validation lives in ReplicationManager and the packet
///          handshake, which read these constants at connect time.
///
/// Versioning policy (semver-like, three numeric fields):
///   major  : breaking change — field removed, enum renumbered, layout
///            changed.  Peers with different majors MUST refuse to talk.
///   minor  : additive change — new optional field appended, new enum
///            case added with a reserved slot.  Older peers MAY accept
///            newer streams by ignoring unknown tails.
///   patch  : purely cosmetic — constants renamed, docs updated, no
///            wire-visible change.  Always compatible.
///
/// These three numbers are *independent* of the engine's own version
/// number (SagaEngine 0.0.6) — a client upgrade might not touch the wire
/// schema at all, in which case `kSchemaVersionMinor` simply stays put.

#pragma once

#include <cstdint>

namespace SagaEngine::Networking {

// ─── Compile-time schema version tuple ──────────────────────────────────────

inline constexpr std::uint16_t kSchemaVersionMajor = 1;
inline constexpr std::uint16_t kSchemaVersionMinor = 0;
inline constexpr std::uint16_t kSchemaVersionPatch = 0;

/// Packed 32-bit form sent over the wire.  Layout:
///   bits 31..16 : major
///   bits 15..8  : minor
///   bits 7..0   : patch
/// 8 bits of headroom on minor/patch is plenty — if we ever need more,
/// we'll have rolled a new major by then.
[[nodiscard]] inline constexpr std::uint32_t PackSchemaVersion(
    std::uint16_t major,
    std::uint16_t minor,
    std::uint16_t patch) noexcept
{
    return (static_cast<std::uint32_t>(major) << 16) |
           (static_cast<std::uint32_t>(minor & 0xFF) << 8) |
            static_cast<std::uint32_t>(patch & 0xFF);
}

inline constexpr std::uint32_t kSchemaVersionPacked =
    PackSchemaVersion(kSchemaVersionMajor, kSchemaVersionMinor, kSchemaVersionPatch);

// ─── Runtime compatibility check ────────────────────────────────────────────

/// Strict compatibility test.  Returns true iff a local peer running the
/// compiled-in version can safely interpret a stream produced by
/// `remotePacked`.  Major mismatch is fatal; minor mismatch is accepted
/// *only if* the remote minor is <= local minor (we can parse older
/// streams; we cannot parse streams with fields we haven't learned yet).
///
/// Note the deliberate asymmetry: this is "can *we* read *them*", not
/// "are we identical".  The handshake calls it from both sides so both
/// peers must agree before any real traffic flows.
[[nodiscard]] inline constexpr bool IsSchemaCompatible(std::uint32_t remotePacked) noexcept
{
    const std::uint16_t remoteMajor = static_cast<std::uint16_t>((remotePacked >> 16) & 0xFFFF);
    const std::uint16_t remoteMinor = static_cast<std::uint16_t>((remotePacked >> 8)  & 0xFF);

    if (remoteMajor != kSchemaVersionMajor)
        return false;

    // Accept older or equal minor versions.  Newer minor means the remote
    // may have added fields we don't know how to skip.
    return remoteMinor <= kSchemaVersionMinor;
}

/// Human-readable form (e.g. "1.0.0") for logs and crash dumps.
/// Intentionally returned by value as a small struct rather than
/// std::string — we log it from signal handlers where heap is off-limits.
struct SchemaVersionStr
{
    char data[16]; ///< Null-terminated.
};

[[nodiscard]] inline SchemaVersionStr FormatSchemaVersion(std::uint32_t packed) noexcept
{
    const auto major = static_cast<std::uint16_t>((packed >> 16) & 0xFFFF);
    const auto minor = static_cast<std::uint16_t>((packed >> 8)  & 0xFF);
    const auto patch = static_cast<std::uint16_t>(packed & 0xFF);

    SchemaVersionStr out{};
    // Manual formatter — no snprintf so this is usable from async-signal
    // contexts.  Writes "M.m.p\0" where each field is up to 5 digits.
    int    pos   = 0;
    auto   write = [&](std::uint16_t v) noexcept
    {
        char  buf[6];
        int   n = 0;
        if (v == 0) { buf[n++] = '0'; }
        else
        {
            while (v > 0 && n < 5) { buf[n++] = static_cast<char>('0' + (v % 10)); v /= 10; }
        }
        while (n > 0 && pos < 15) { out.data[pos++] = buf[--n]; }
    };

    write(major);
    if (pos < 15) out.data[pos++] = '.';
    write(minor);
    if (pos < 15) out.data[pos++] = '.';
    write(patch);
    out.data[pos] = '\0';
    return out;
}

} // namespace SagaEngine::Networking
