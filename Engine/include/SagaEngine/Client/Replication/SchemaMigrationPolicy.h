/// @file SchemaMigrationPolicy.h
/// @file Schema version negotiation and backward compatibility policy.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Defines the rules for handling schema version mismatches
///          between client and server, including forward-compatible
///          component skipping, deprecated component handling, and
///          the deprecation timeline for old schema versions.
///
/// Version format: major.minor.patch (packed into uint32: 10.10.12 bits)
///   - major: breaking change (incompatible)
///   - minor: additive change (forward-compatible)
///   - patch: bug fix (compatible)
///
/// Policy:
///   - Major mismatch → reject connection, disconnect client.
///   - Server minor > client minor → skip unknown component types.
///   - Client minor > server minor → ignore extra capabilities.
///   - Patch: always use the server's patch level.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEngine::Client::Replication {

// ─── Schema version ────────────────────────────────────────────────────────

/// Packed schema version: 10 bits major, 10 bits minor, 12 bits patch.
struct SchemaVersion
{
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint16_t patch = 0;

    [[nodiscard]] std::uint32_t Pack() const noexcept;
    [[nodiscard]] std::string Format() const noexcept;
};

/// Unpack a uint32 into a SchemaVersion.
[[nodiscard]] SchemaVersion UnpackVersion(std::uint32_t packed) noexcept;

// ─── Compatibility result ──────────────────────────────────────────────────

enum class CompatibilityResult : std::uint8_t
{
    Compatible,             ///< Full compatibility — proceed normally.
    ForwardCompatible,      ///< Server has newer minor; skip unknown types.
    BackwardCompatible,     ///< Client has newer minor; ignore extras.
    Incompatible,           ///< Major mismatch — disconnect required.
};

// ─── Migration policy ──────────────────────────────────────────────────────

/// Schema version compatibility checker and migration coordinator.
class SchemaMigrationPolicy
{
public:
    SchemaMigrationPolicy() = default;

    /// Set the client's supported version range.
    void SetClientVersion(SchemaVersion minSupported, SchemaVersion current) noexcept;

    /// Check compatibility with the server's advertised version.
    ///
    /// @param serverVersion  The server's schema version from handshake.
    /// @returns Compatibility result and negotiated common version.
    [[nodiscard]] CompatibilityResult CheckCompatibility(
        SchemaVersion serverVersion,
        SchemaVersion* outNegotiatedVersion) noexcept;

    /// Return true if a component type should be skipped during
    /// decode because the client does not recognise it.
    ///
    /// @param serverMinor  The server's minor version that introduced this type.
    /// @param clientMinor  The client's minor version (max supported).
    [[nodiscard]] bool ShouldSkipComponent(std::uint16_t serverMinor,
                                            std::uint16_t clientMinor) const noexcept;

    /// Return the negotiated common version after CheckCompatibility.
    [[nodiscard]] SchemaVersion GetNegotiatedVersion() const noexcept { return negotiatedVersion_; }

private:
    SchemaVersion minSupported_{};
    SchemaVersion current_{};
    SchemaVersion negotiatedVersion_{};
};

} // namespace SagaEngine::Client::Replication
