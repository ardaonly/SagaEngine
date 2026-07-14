/// @file ISecureChannel.h
/// @brief Pluggable encryption/authentication layer between transport and packet pipeline.
///
/// Layer  : SagaServer / Networking / Security
/// Purpose: The wire path today is `UdpTransport → Packet → dispatcher`.
///          A production MMO can't ship with that: login credentials,
///          chat, and trade actions must be confidential on the wire.
///          This header defines the abstract secure-channel interface
///          the rest of the networking stack talks to; the concrete
///          backend (OpenSSL DTLS, Noise Protocol, game-specific
///          symmetric channel) plugs in at service startup.
///
/// Why an interface and not just OpenSSL directly:
///   1. OpenSSL is an enormous external dependency.  Until the build
///      system gains OpenSSL we still need *something* in the hot
///      path — the `NullSecureChannel` below is that placeholder and
///      keeps every call site compile-clean.
///   2. Some deployments run inside an internal datacentre where the
///      operator has already terminated TLS at a load balancer.  In
///      that case `NullSecureChannel` is the right answer permanently.
///   3. Different games want different trade-offs (AES-GCM for low
///      overhead, ChaCha20-Poly1305 for mobile clients without AES-NI).
///      A pluggable interface turns that into configuration.
///
/// Threading:
///   The interface contract says: one `ISecureChannel` instance per
///   connection, never shared.  That lets backends keep per-connection
///   crypto state (nonces, sequence counters) without mutex overhead.
///   If the transport dispatches encryption on a thread pool it must
///   use a `SecureChannelFactory` to materialise per-connection state.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace SagaEngine::Networking {

// ─── Result codes ────────────────────────────────────────────────────────────

/// Return value on every encrypt / decrypt operation.  Using an enum
/// lets callers distinguish "wrong key" from "truncated buffer" from
/// "nothing to do right now" — three very different responses up the
/// stack (disconnect vs retry vs wait for more data).
enum class SecureChannelResult : std::uint8_t
{
    Ok = 0,
    NeedMoreData,      ///< Handshake not finished yet, feed more bytes.
    HandshakeInProgress, ///< Produced some bytes but not yet ready for traffic.
    BufferTooSmall,
    AuthenticationFailed, ///< MAC mismatch — peer is corrupted or malicious.
    CodecMismatch,
    InternalError,
};

/// Handshake state visible to the transport.  Drives the connection
/// state machine in ServerConnection.
enum class HandshakeState : std::uint8_t
{
    NotStarted = 0,
    InProgress,
    Established,  ///< Keys derived; Encrypt/Decrypt may be called.
    Failed,
    Closed
};

// ─── Interface ───────────────────────────────────────────────────────────────

/// Abstract encrypted channel.  One instance per connection.  The
/// transport feeds raw inbound bytes via `HandleInbound` until
/// `State() == Established`, then switches to `Decrypt`/`Encrypt` for
/// the traffic phase.
///
/// Memory ownership: all `std::vector` arguments are owned by the
/// caller.  Backends may append to `output` but must not retain
/// references past the call.
class ISecureChannel
{
public:
    virtual ~ISecureChannel() = default;

    // ── Identity ──────────────────────────────────────────────────────────

    /// Stable human-readable backend name.  Used by logs and the
    /// `/netinfo` admin command.  Must be cheap; called at log-warning
    /// severity.
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;

    /// Bit flags describing the guarantees this backend provides.  The
    /// transport logs these at connect time so operators can tell at a
    /// glance whether they're running a dev insecure channel against a
    /// production server — a very easy footgun without this bit.
    enum Features : std::uint32_t
    {
        kFeatureConfidentiality = 1u << 0, ///< Payload is encrypted.
        kFeatureIntegrity       = 1u << 1, ///< Messages are authenticated.
        kFeatureForwardSecrecy  = 1u << 2, ///< Key compromise does not expose history.
        kFeatureReplayProtected = 1u << 3, ///< Per-frame nonces guarantee freshness.
    };
    [[nodiscard]] virtual std::uint32_t FeatureFlags() const noexcept = 0;

    [[nodiscard]] virtual HandshakeState State() const noexcept = 0;

    // ── Handshake ─────────────────────────────────────────────────────────

    /// Drive the handshake state machine.  `inbound` may be empty — the
    /// backend is expected to produce the first handshake message from
    /// thin air when called on the connecting side.  `outbound` is
    /// *appended*, not replaced, so the transport can collect multiple
    /// flights in one vector.
    [[nodiscard]] virtual SecureChannelResult HandleInbound(
        const std::uint8_t*       inbound,
        std::size_t               inboundSize,
        std::vector<std::uint8_t>& outbound) = 0;

    // ── Traffic phase ─────────────────────────────────────────────────────

    /// Encrypt a full application-level packet.  Must be called only
    /// after `State() == Established`.  `output` is replaced, not
    /// appended — the transport hands the vector straight to `send`.
    [[nodiscard]] virtual SecureChannelResult Encrypt(
        const std::uint8_t*       plaintext,
        std::size_t               plaintextSize,
        std::vector<std::uint8_t>& output) = 0;

    /// Decrypt a single packet.  Returns `AuthenticationFailed` on MAC
    /// mismatch; the transport must drop the connection immediately on
    /// that result (never retry).
    [[nodiscard]] virtual SecureChannelResult Decrypt(
        const std::uint8_t*       ciphertext,
        std::size_t               ciphertextSize,
        std::vector<std::uint8_t>& output) = 0;

    /// Tear down the channel.  Optional but recommended — some backends
    /// emit a "close_notify" record so the peer knows the disconnect
    /// was clean and not a truncation attack.
    virtual void Close() = 0;
};

using SecureChannelPtr = std::unique_ptr<ISecureChannel>;

// ─── Factory ─────────────────────────────────────────────────────────────────

/// Produces `ISecureChannel` instances on demand.  One factory per
/// process, shared across every ServerConnection, owned by ZoneServer.
class ISecureChannelFactory
{
public:
    virtual ~ISecureChannelFactory() = default;

    /// Called for every new ServerConnection.  `role` chooses between
    /// the server and client side of the handshake.
    enum class Role : std::uint8_t { Server, Client };
    [[nodiscard]] virtual SecureChannelPtr Create(Role role) = 0;

    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

using SecureChannelFactoryPtr = std::shared_ptr<ISecureChannelFactory>;

// ─── Null backend ────────────────────────────────────────────────────────────

/// Pass-through "encryption" for dev builds and deployments terminating
/// TLS at an upstream load balancer.  Every method succeeds without
/// touching the bytes; `FeatureFlags()` returns 0 so operators see
/// "Features: (none)" in the startup log and can't miss it.
class NullSecureChannel final : public ISecureChannel
{
public:
    [[nodiscard]] const char* BackendName() const noexcept override { return "Null"; }
    [[nodiscard]] std::uint32_t FeatureFlags() const noexcept override { return 0; }
    [[nodiscard]] HandshakeState State() const noexcept override { return state_; }

    [[nodiscard]] SecureChannelResult HandleInbound(
        const std::uint8_t*       /*inbound*/,
        std::size_t               /*inboundSize*/,
        std::vector<std::uint8_t>& /*outbound*/) override
    {
        state_ = HandshakeState::Established;
        return SecureChannelResult::Ok;
    }

    [[nodiscard]] SecureChannelResult Encrypt(
        const std::uint8_t*       plaintext,
        std::size_t               plaintextSize,
        std::vector<std::uint8_t>& output) override
    {
        output.assign(plaintext, plaintext + plaintextSize);
        return SecureChannelResult::Ok;
    }

    [[nodiscard]] SecureChannelResult Decrypt(
        const std::uint8_t*       ciphertext,
        std::size_t               ciphertextSize,
        std::vector<std::uint8_t>& output) override
    {
        output.assign(ciphertext, ciphertext + ciphertextSize);
        return SecureChannelResult::Ok;
    }

    void Close() override { state_ = HandshakeState::Closed; }

private:
    HandshakeState state_ = HandshakeState::NotStarted;
};

/// Factory that hands out `NullSecureChannel` instances.  Use in unit
/// tests and in deployments where TLS is terminated upstream.
class NullSecureChannelFactory final : public ISecureChannelFactory
{
public:
    [[nodiscard]] SecureChannelPtr Create(Role /*role*/) override
    {
        return std::make_unique<NullSecureChannel>();
    }
    [[nodiscard]] const char* BackendName() const noexcept override { return "Null"; }
};

} // namespace SagaEngine::Networking
