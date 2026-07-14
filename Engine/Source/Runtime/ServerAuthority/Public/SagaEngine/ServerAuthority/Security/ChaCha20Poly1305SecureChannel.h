/// @file ChaCha20Poly1305SecureChannel.h
/// @brief Dependency-free ChaCha20-Poly1305 AEAD backend for ISecureChannel.
///
/// Layer  : SagaServer / Networking / Security
/// Purpose: Ship a real, production-capable secure channel *today*,
///          without pulling OpenSSL / libsodium into the build.  The
///          backend implements ChaCha20-Poly1305 per RFC 8439 — the
///          same AEAD TLS 1.3 uses on mobile hardware without AES-NI.
///
///          The channel pairs with a pre-shared 32-byte session key.
///          For internal cluster traffic (zone ↔ zone, zone ↔ gateway)
///          the PSK is distributed by the config system; for external
///          client traffic the PSK is derived from a secure token
///          service that sits outside this header.  The key-exchange
///          design is intentionally kept out of scope here — any
///          future X25519 / Noise layer will deliver a 32-byte key
///          and the channel will not notice the difference.
///
/// Wire format per encrypted record:
///
///     bytes 0..7  : little-endian uint64 record sequence number
///     bytes 8..N  : ChaCha20 ciphertext (same length as plaintext)
///     bytes N..N+16 : Poly1305 tag over (aad | ciphertext)
///
/// The sequence number doubles as the nonce low 64 bits; the nonce
/// high 32 bits are a channel-wide constant (`nonceSalt`).  Replay
/// protection is enforced by the sliding-window `ReplayGuard` that
/// lives beside this header.
///
/// Threading:
///   As mandated by `ISecureChannel`, one instance per connection
///   and never shared.  The sequence counter is not atomic because
///   the contract says no sharing.
///
/// What this header is NOT:
///   - Not a timing-hardened implementation.  The Poly1305 tag is
///     constant-time, but the surrounding framing is not.  Internal
///     cluster traffic is fine with that; external client traffic
///     should use the future OpenSSL backend for defence-in-depth.
///   - Not a key-exchange protocol.  The session key must already
///     be established by the caller before the channel is handed to
///     the transport.

#pragma once

#include "SagaServer/Networking/Security/ISecureChannel.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace SagaEngine::Networking {

// ─── Constants ────────────────────────────────────────────────────────────

/// ChaCha20 key size in bytes (256 bits).  Non-negotiable — ChaCha20
/// has no shorter key variant.
inline constexpr std::size_t kChaChaKeyBytes = 32;

/// ChaCha20 nonce size in bytes (96 bits, per RFC 8439).  The low
/// 64 bits are the record sequence counter; the high 32 bits are
/// the per-channel salt, chosen at channel creation time.
inline constexpr std::size_t kChaChaNonceBytes = 12;

/// Poly1305 authentication tag size in bytes.  Added to the tail of
/// every encrypted record.
inline constexpr std::size_t kPoly1305TagBytes = 16;

/// On-the-wire framing overhead per encrypted record: sequence (8) +
/// tag (16).  Callers size their MTU budgets using this constant so
/// a 1200-byte plaintext never overflows the transport MTU.
inline constexpr std::size_t kChaChaFrameOverhead = 8 + kPoly1305TagBytes;

// ─── AEAD primitives ──────────────────────────────────────────────────────

/// Encrypt `plaintextSize` bytes with ChaCha20-Poly1305.  `out`
/// must have room for `plaintextSize + kPoly1305TagBytes` bytes; the
/// cipher writes the ciphertext followed by the authentication tag.
/// `aad` is optional additional authenticated data that is MAC'd
/// but not encrypted (the caller typically passes the record header
/// here).  Always returns true — the AEAD itself has no error path
/// once the buffer size has been validated by the caller.
bool ChaCha20Poly1305Seal(
    const std::uint8_t* key,     ///< 32-byte session key.
    const std::uint8_t* nonce,   ///< 12-byte nonce.
    const std::uint8_t* aad,     ///< Optional AAD (may be null).
    std::size_t         aadSize,
    const std::uint8_t* plaintext,
    std::size_t         plaintextSize,
    std::uint8_t*       out) noexcept;

/// Decrypt + verify.  `in` points to `ciphertextSize + kPoly1305TagBytes`
/// bytes; the final 16 bytes are the tag.  Returns true if the tag
/// verifies; on tag failure returns false and leaves `out` partially
/// written (the caller must zero it on failure if secrecy of the
/// partial plaintext matters).
[[nodiscard]] bool ChaCha20Poly1305Open(
    const std::uint8_t* key,
    const std::uint8_t* nonce,
    const std::uint8_t* aad,
    std::size_t         aadSize,
    const std::uint8_t* ciphertext,
    std::size_t         ciphertextSize,
    const std::uint8_t* tag,
    std::uint8_t*       out) noexcept;

// ─── Config ───────────────────────────────────────────────────────────────

/// Per-channel configuration.  Callers fill this in before handing
/// it to the factory; the channel copies every field so callers may
/// reuse / free the source object immediately.
struct ChaCha20ChannelConfig
{
    /// 32-byte pre-shared session key.  The caller is responsible
    /// for zeroing this after construction if the key material is
    /// sensitive beyond channel lifetime.
    std::array<std::uint8_t, kChaChaKeyBytes> sessionKey{};

    /// 32-bit nonce salt — typically derived from the connection
    /// id.  Pairs with the sequence counter to produce a 96-bit
    /// nonce that is unique across every record for the lifetime
    /// of the channel.
    std::uint32_t nonceSalt = 0;

    /// Maximum plaintext size in bytes the channel will accept in
    /// `Encrypt`.  Set by the transport to the MTU minus framing
    /// overhead; the channel rejects oversize requests with
    /// `BufferTooSmall`.
    std::size_t maxPlaintextBytes = 4096;
};

// ─── Channel ──────────────────────────────────────────────────────────────

/// ChaCha20-Poly1305 `ISecureChannel` implementation.  One per
/// connection; the handshake is a no-op because the PSK is
/// delivered out-of-band.  `State()` jumps straight to
/// `Established` on construction.
class ChaCha20Poly1305SecureChannel final : public ISecureChannel
{
public:
    explicit ChaCha20Poly1305SecureChannel(const ChaCha20ChannelConfig& config) noexcept;

    // ── Identity ──────────────────────────────────────────────────────────

    [[nodiscard]] const char* BackendName() const noexcept override
    {
        return "ChaCha20-Poly1305";
    }

    [[nodiscard]] std::uint32_t FeatureFlags() const noexcept override
    {
        // Confidentiality + integrity + per-frame replay protection.
        // No forward secrecy — the key is a long-lived PSK until the
        // future handshake layer adds ephemeral key exchange.
        return kFeatureConfidentiality |
               kFeatureIntegrity       |
               kFeatureReplayProtected;
    }

    [[nodiscard]] HandshakeState State() const noexcept override { return state_; }

    // ── Handshake (no-op) ────────────────────────────────────────────────

    [[nodiscard]] SecureChannelResult HandleInbound(
        const std::uint8_t*        inbound,
        std::size_t                inboundSize,
        std::vector<std::uint8_t>& outbound) override;

    // ── Traffic phase ────────────────────────────────────────────────────

    [[nodiscard]] SecureChannelResult Encrypt(
        const std::uint8_t*        plaintext,
        std::size_t                plaintextSize,
        std::vector<std::uint8_t>& output) override;

    [[nodiscard]] SecureChannelResult Decrypt(
        const std::uint8_t*        ciphertext,
        std::size_t                ciphertextSize,
        std::vector<std::uint8_t>& output) override;

    void Close() override { state_ = HandshakeState::Closed; }

    // ── Introspection ─────────────────────────────────────────────────────

    /// Most recently accepted decrypt sequence number.  Exposed for
    /// tests and for the diagnostics overlay that surfaces replay
    /// protection coverage.
    [[nodiscard]] std::uint64_t LastReceivedSequence() const noexcept { return rxSequence_; }

    /// Next sequence number `Encrypt` will stamp onto an outgoing
    /// record.  Used by tests that want to verify monotonicity.
    [[nodiscard]] std::uint64_t NextSendSequence() const noexcept { return txSequence_; }

private:
    /// Compose a 12-byte nonce from `nonceSalt` + sequence counter
    /// into `out`.  The high 4 bytes are the salt in network order;
    /// the low 8 bytes are the sequence counter in little-endian,
    /// matching the RFC 8439 nonce layout.
    void BuildNonce(std::uint64_t sequence, std::uint8_t out[kChaChaNonceBytes]) const noexcept;

    ChaCha20ChannelConfig config_;
    HandshakeState        state_      = HandshakeState::NotStarted;
    std::uint64_t         txSequence_ = 0;
    std::uint64_t         rxSequence_ = 0;
};

// ─── Factory ──────────────────────────────────────────────────────────────

/// Factory that produces `ChaCha20Poly1305SecureChannel` instances
/// from a caller-supplied configuration.  Tests wire this up with a
/// deterministic key; production wires it up with a derived session
/// key from the account service.
class ChaCha20SecureChannelFactory final : public ISecureChannelFactory
{
public:
    explicit ChaCha20SecureChannelFactory(ChaCha20ChannelConfig config) noexcept
        : config_(std::move(config))
    {
    }

    [[nodiscard]] SecureChannelPtr Create(Role /*role*/) override
    {
        return std::make_unique<ChaCha20Poly1305SecureChannel>(config_);
    }

    [[nodiscard]] const char* BackendName() const noexcept override
    {
        return "ChaCha20-Poly1305";
    }

    /// Mutate the configuration used for every subsequent `Create`.
    /// Called by the admin command that rotates session keys on a
    /// scheduled interval.  Existing channels keep their old key
    /// until they are torn down — the channel is stateless w.r.t.
    /// the factory.
    void UpdateConfig(const ChaCha20ChannelConfig& config) noexcept { config_ = config; }

private:
    ChaCha20ChannelConfig config_;
};

} // namespace SagaEngine::Networking
