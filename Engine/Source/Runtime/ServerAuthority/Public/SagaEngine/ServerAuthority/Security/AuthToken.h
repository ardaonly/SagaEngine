/// @file AuthToken.h
/// @brief Opaque authentication token type + validator interface.
///
/// Layer  : SagaServer / Networking / Security
/// Purpose: SagaEngine needs a neutral abstraction for "is this client who
///          they say they are?" that the handshake layer can consult without
///          caring whether the backing implementation is JWT, Paseto, HMAC
///          macaroons, or a custom scheme.  This header is the boundary.
///
/// Design:
///   - `AuthToken` is an opaque byte blob with a content-type tag.  The
///     server never interprets the bytes — only the `IAuthTokenValidator`
///     implementation does, and it is chosen at server start-up.
///   - Validation is synchronous on a handshake thread; backends that need
///     to call out to an identity provider should cache results and fall
///     back to a short deny decision rather than blocking the handshake.
///   - `AuthContext` is what the validator returns on success — an account
///     identifier plus a coarse role flag the gameplay layer can key off.
///
/// Integration:
///   - ServerConnection state machine calls `IAuthTokenValidator::Validate`
///     once during the "authenticate" phase.  On success it stores the
///     returned `AuthContext` and transitions to "connected".
///   - The concrete backend (JWT, custom, dev stub) is installed at
///     ZoneServer construction time via dependency injection.
///
/// Non-goals:
///   - No cryptography in this header — that lives in the concrete backend.
///   - No session establishment — replay-guard / TLS handle that layer.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Networking::Security
{

// ─── Account identifier ──────────────────────────────────────────────────────

/// Stable, globally unique account identifier resolved by the validator.
/// The numeric form is used as a hash key in fast paths (session table,
/// presence); the string form is kept for log output and audit trails.
struct AccountId
{
    std::uint64_t numeric = 0;   ///< Packed 64-bit id, 0 = invalid.
    std::string   display;       ///< Human-readable account name, e.g. UUID.

    [[nodiscard]] bool IsValid() const noexcept { return numeric != 0; }
};

// ─── Coarse authorisation roles ──────────────────────────────────────────────

/// Coarse permission tier returned by the validator.  Fine-grained game
/// permissions live in the gameplay layer; this enum only drives access to
/// engine-level capabilities (zone admin, debug RPCs).
enum class AuthRole : std::uint8_t
{
    Player    = 0,  ///< Standard game client.
    Gm        = 1,  ///< Game master — can issue privileged commands.
    Admin     = 2,  ///< Full server administration access.
    Bot       = 3,  ///< Automated test client; rate-limited differently.
    Banned    = 255 ///< Explicit deny — treat as failed validation.
};

// ─── AuthToken wire blob ─────────────────────────────────────────────────────

/// Opaque authentication payload as received on the handshake packet.
/// The engine does not interpret the bytes; the validator does.
struct AuthToken
{
    /// Short scheme tag, e.g. "jwt", "paseto", "dev".  Used by the
    /// validator registry to dispatch to the correct backend.
    std::string scheme;

    /// Raw token bytes as received on the wire.  Ownership transferred to
    /// the validator on a successful Validate call.
    std::vector<std::uint8_t> payload;

    /// Optional audience hint from the handshake (which zone the client is
    /// trying to join).  The validator can reject tokens that were minted
    /// for a different audience even if the signature is otherwise valid.
    std::string audience;
};

// ─── AuthContext (successful validation result) ──────────────────────────────

/// What the validator returns on a successful Validate call.  The server
/// stores this on the ServerConnection and hands it to the gameplay layer.
struct AuthContext
{
    AccountId account;
    AuthRole  role = AuthRole::Player;

    /// Token issue time in the shared millisecond epoch used by the
    /// replay guard.  0 means "unknown".
    std::int64_t issuedAtMs = 0;

    /// Token expiry time in the same epoch.  The server must re-validate
    /// (or reject) after this point.  0 means "never expires".
    std::int64_t expiresAtMs = 0;
};

// ─── Validation decision ─────────────────────────────────────────────────────

/// Explicit reason code returned alongside a success/failure flag.  Helps
/// distinguish "bad token" from "identity service unreachable" so the
/// gateway can retry vs fail fast.
enum class ValidationError : std::uint8_t
{
    None                = 0, ///< Success — use the AuthContext.
    UnknownScheme       = 1, ///< No validator registered for token.scheme.
    MalformedPayload    = 2, ///< Could not parse the token bytes.
    SignatureInvalid    = 3, ///< Cryptographic check failed.
    Expired             = 4, ///< Token valid but past its expiry.
    NotYetValid         = 5, ///< Token not yet active (nbf in future).
    AudienceMismatch    = 6, ///< Token minted for a different zone / server.
    AccountBanned       = 7, ///< Account is on the ban list.
    BackendUnavailable  = 8  ///< Validator could not reach its identity source.
};

struct ValidationResult
{
    ValidationError error = ValidationError::None;
    AuthContext     context;

    [[nodiscard]] bool IsOk() const noexcept
    {
        return error == ValidationError::None;
    }
};

// ─── Validator interface ─────────────────────────────────────────────────────

/// Abstract validator.  One concrete implementation per backend scheme.
/// Implementations MUST be thread-safe — ZoneServer calls `Validate` from
/// the handshake pool and may run it in parallel on many connections.
class IAuthTokenValidator
{
public:
    virtual ~IAuthTokenValidator() = default;

    /// Check a token.  Synchronous but expected to complete in < 10 ms;
    /// backends that need to consult a remote service must cache.
    /// `serverNowMs` is the server's current wall-clock in the shared epoch,
    /// used for expiry / not-before checks.
    [[nodiscard]] virtual ValidationResult Validate(
        const AuthToken& token,
        std::int64_t     serverNowMs) noexcept = 0;

    /// Short identifier for this backend — shows up in logs and stats.
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

/// Shared-ownership handle used by the server to hold onto a validator.
/// shared_ptr because several subsystems (handshake, admin RPCs) may share
/// the same instance without an explicit lifetime owner.
using AuthTokenValidatorPtr = std::shared_ptr<IAuthTokenValidator>;

} // namespace SagaEngine::Networking::Security
