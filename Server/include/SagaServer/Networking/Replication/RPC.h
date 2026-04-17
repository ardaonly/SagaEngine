/// @file RPC.h
/// @brief Server-side RPC (Remote Procedure Call) system with codec and dispatch table.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: The RPC system allows clients to request server-side actions
///          (e.g., "cast spell", "open trade", "send chat message").  Each
///          RPC is a named function with typed parameters that are serialized
///          over the network, dispatched on the server, and optionally
///          return a result to the client.
///
/// Design rules:
///   - RPCs are registered with a string name and a handler function
///   - The dispatch table maps RPC name → handler (O(1) lookup)
///   - Parameters are encoded in a compact binary format (type-length-value)
///   - Authentication gates can require the client to be logged in
///   - Rate limiting prevents RPC spam (per-client cooldown)
///   - RPC results are serialized back to the client as a response packet
///
/// Wire format (client → server):
///   [RPC header: 8 bytes]
///     nameLength(2) | name(N bytes) | argCount(1) | args...
///   [Per argument:]
///     typeTag(1) | length(2) | data(N bytes)
///
/// Wire format (server → client):
///   [RPC response header: 6 bytes]
///     rpcId(4) | statusCode(1) | resultData...
///
/// Security:
///   - Authentication is checked before dispatch
///   - Rate limiting is per-client, per-RPC (prevents abuse)
///   - Malformed packets are dropped and logged

#pragma once

#include "SagaEngine/ECS/Entity.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaServer {

// ─── RPC constants ──────────────────────────────────────────────────────

static constexpr uint16_t kMaxRPCNameLength     = 64;
static constexpr uint8_t  kMaxRPCArguments       = 16;
static constexpr uint32_t kMaxRPCPayloadBytes    = 4096;
static constexpr uint32_t kDefaultRPCCooldownMs  = 100;  ///< Per-client, per-RPC.

// ─── RPC argument type tags ─────────────────────────────────────────────

enum class RPCArgType : uint8_t
{
    None    = 0,
    Int8    = 1,
    Int16   = 2,
    Int32   = 3,
    Int64   = 4,
    UInt8   = 5,
    UInt16  = 6,
    UInt32  = 7,
    UInt64  = 8,
    Float   = 9,
    Double  = 10,
    Bool    = 11,
    String  = 12,
    Entity  = 13,
    Blob    = 14,  ///< Raw binary blob.
};

/// Size in bytes of a fixed-size argument type (returns 0 for variable-size types).
[[nodiscard]] inline uint16_t RPCArgTypeSize(RPCArgType type) noexcept
{
    switch (type)
    {
        case RPCArgType::Int8:  case RPCArgType::UInt8:  return 1;
        case RPCArgType::Int16: case RPCArgType::UInt16: return 2;
        case RPCArgType::Int32: case RPCArgType::UInt32:
        case RPCArgType::Float:                          return 4;
        case RPCArgType::Int64: case RPCArgType::UInt64:
        case RPCArgType::Double:                         return 8;
        case RPCArgType::Bool:                           return 1;
        case RPCArgType::String: case RPCArgType::Blob:  return 0;  // Variable-length.
        case RPCArgType::Entity:                         return 4;
        default:                                         return 0;
    }
}

// ─── RPC argument ───────────────────────────────────────────────────────

/// A single typed argument for an RPC call.
struct RPCArgument
{
    RPCArgType          type   = RPCArgType::None;
    std::vector<uint8_t> data;  ///< Binary representation of the argument.

    /// Decode an int32_t from the argument.
    [[nodiscard]] int32_t AsInt32() const noexcept
    {
        if (type != RPCArgType::Int32 || data.size() < 4)
            return 0;
        int32_t value;
        std::memcpy(&value, data.data(), 4);
        return value;
    }

    /// Decode a float from the argument.
    [[nodiscard]] float AsFloat() const noexcept
    {
        if (type != RPCArgType::Float || data.size() < 4)
            return 0.0f;
        float value;
        std::memcpy(&value, data.data(), 4);
        return value;
    }

    /// Decode a string from the argument.
    [[nodiscard]] std::string AsString() const noexcept
    {
        if (type != RPCArgType::String)
            return {};
        return std::string(reinterpret_cast<const char*>(data.data()), data.size());
    }

    /// Decode an entity ID from the argument.
    [[nodiscard]] uint32_t AsEntity() const noexcept
    {
        if (type != RPCArgType::Entity || data.size() < 4)
            return 0;
        uint32_t value;
        std::memcpy(&value, data.data(), 4);
        return value;
    }
};

// ─── RPC request (decoded) ──────────────────────────────────────────────

/// A fully decoded RPC request from a client.
struct RPCRequest
{
    std::string             rpcName;           ///< Registered RPC function name.
    uint64_t                clientId  = 0;     ///< Client session ID.
    uint32_t                rpcId     = 0;     ///< Unique call identifier (for response matching).
    std::vector<RPCArgument> arguments;         ///< Typed arguments.
};

// ─── RPC response ───────────────────────────────────────────────────────

enum class RPCStatusCode : uint8_t
{
    Ok          = 0,
    NotFound    = 1,  ///< RPC name not registered.
    AuthFailed  = 2,  ///< Client not authenticated.
    RateLimited = 3,  ///< Client exceeded RPC rate limit.
    BadArgs     = 4,  ///< Argument count or type mismatch.
    InternalError = 5,///< Server-side handler error.
};

struct RPCResponse
{
    uint32_t                rpcId       = 0;
    RPCStatusCode           status      = RPCStatusCode::Ok;
    std::vector<uint8_t>    resultData;  ///< Handler-specific result bytes.
};

// ─── RPC handler signature ──────────────────────────────────────────────

/// RPC handler function type.
/// @param clientId  The client session that initiated the request.
/// @param request   Decoded RPC request (arguments validated by the codec).
/// @param outResponse  Output: response to send back to the client.
/// @return true if the handler succeeded (status = Ok); false for errors.
using RPCHandlerFn = std::function<bool(
    uint64_t       clientId,
    const RPCRequest& request,
    RPCResponse&   outResponse)>;

// ─── RPC codec ──────────────────────────────────────────────────────────

/// Encodes and decodes RPC requests/responses to/from binary wire format.
///
/// The codec is stateless and thread-safe (no internal mutable state).
class RPCCodec
{
public:
    RPCCodec() noexcept = default;
    ~RPCCodec() noexcept = default;

    /// Encode an RPC request into a binary buffer.
    /// @param request  Decoded request.
    /// @param outBuffer Output: encoded bytes.
    /// @return true if encoding succeeded (name + args within size limits).
    [[nodiscard]] static bool EncodeRequest(const RPCRequest& request,
                                             std::vector<uint8_t>& outBuffer) noexcept;

    /// Decode an RPC request from a binary buffer.
    /// @param data   Raw bytes received from client.
    /// @param size   Number of bytes in the buffer.
    /// @param clientId  Client session ID (attached to decoded request).
    /// @param rpcId  Unique call ID (attached to decoded request).
    /// @param outRequest Output: decoded request.
    /// @return false if the buffer is malformed or exceeds size limits.
    [[nodiscard]] static bool DecodeRequest(const uint8_t* data, std::size_t size,
                                             uint64_t clientId, uint32_t rpcId,
                                             RPCRequest& outRequest) noexcept;

    /// Encode an RPC response into a binary buffer.
    /// @param response  Decoded response.
    /// @param outBuffer Output: encoded bytes.
    /// @return true if encoding succeeded.
    [[nodiscard]] static bool EncodeResponse(const RPCResponse& response,
                                              std::vector<uint8_t>& outBuffer) noexcept;

    /// Decode an RPC response from a binary buffer.
    /// @param data   Raw bytes received from server.
    /// @param size   Number of bytes.
    /// @param outResponse Output: decoded response.
    /// @return false if the buffer is malformed.
    [[nodiscard]] static bool DecodeResponse(const uint8_t* data, std::size_t size,
                                              RPCResponse& outResponse) noexcept;

private:
    // Internal helpers for TLV (type-length-value) encoding.
    static bool WriteArgument(std::vector<uint8_t>& buffer, const RPCArgument& arg) noexcept;
    static bool ReadArgument(const uint8_t* data, std::size_t size, std::size_t& offset,
                             RPCArgument& outArg) noexcept;
};

// ─── RPC rate limiter ───────────────────────────────────────────────────

/// Per-client, per-RPC rate limiter using a sliding window.
struct RPCRateLimiter
{
    uint32_t cooldownMs = kDefaultRPCCooldownMs;  ///< Minimum ms between calls.
    uint64_t lastCallTimeUs = 0;  ///< Wall-clock microseconds of last call.

    /// Check if an RPC call is allowed right now.
    [[nodiscard]] bool AllowCall(uint64_t nowUs) const noexcept
    {
        if (cooldownMs == 0)
            return true;  // No rate limit.

        const uint64_t cooldownUs = static_cast<uint64_t>(cooldownMs) * 1000u;
        return (nowUs - lastCallTimeUs) >= cooldownUs;
    }

    /// Record an RPC call (updates lastCallTimeUs).
    void RecordCall(uint64_t nowUs) noexcept
    {
        lastCallTimeUs = nowUs;
    }
};

// ─── RPC dispatch table ─────────────────────────────────────────────────

/// Registered RPC function metadata.
struct RPCEntry
{
    std::string     name;
    RPCHandlerFn    handler;
    bool            requiresAuth  = true;   ///< Client must be authenticated.
    uint32_t        cooldownMs    = kDefaultRPCCooldownMs;  ///< Rate limit.
};

/// Dispatch table mapping RPC name → handler.
///
/// Thread model: registration at boot time (single-threaded); dispatch is
/// read-only (lock-free) during runtime.
class RPCDispatch
{
public:
    RPCDispatch() noexcept = default;
    ~RPCDispatch() noexcept = default;

    RPCDispatch(const RPCDispatch&)            = delete;
    RPCDispatch& operator=(const RPCDispatch&) = delete;

    /// Register an RPC handler.
    /// @param name     Unique RPC name (case-sensitive; max kMaxRPCNameLength chars).
    /// @param handler  Handler function to call when the RPC is invoked.
    /// @param requiresAuth  If true, only authenticated clients can call this RPC.
    /// @param cooldownMs  Per-client rate limit in milliseconds (0 = unlimited).
    /// @return true if registration succeeded; false if name already taken.
    bool Register(const char* name, RPCHandlerFn handler,
                  bool requiresAuth = true, uint32_t cooldownMs = kDefaultRPCCooldownMs) noexcept;

    /// Unregister an RPC handler.
    /// @param name  RPC name.
    /// @return true if the handler was removed.
    bool Unregister(const char* name) noexcept;

    /// Dispatch an RPC request to the registered handler.
    /// @param clientId   Client session ID (for rate limiting and auth checks).
    /// @param clientAuth  True if the client is authenticated.
    /// @param request    Decoded RPC request.
    /// @param outResponse  Output: response from the handler.
    /// @return true if a handler was found and called (regardless of success/failure).
    bool Dispatch(uint64_t clientId, bool clientAuth,
                  const RPCRequest& request, RPCResponse& outResponse) noexcept;

    /// Get the number of registered RPCs.
    [[nodiscard]] uint32_t Count() const noexcept;

    /// Get all registered RPC names (for diagnostics).
    [[nodiscard]] std::vector<std::string> GetNames() const noexcept;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, RPCEntry> m_handlers;

    // Per-client rate limiting: (clientId, rpcName) → rate limiter state.
    mutable std::mutex m_rateMutex;
    std::unordered_map<uint64_t, std::unordered_map<std::string, RPCRateLimiter>> m_rates;
};

// ─── Backwards-compatible macros ────────────────────────────────────────

/// Register an RPC handler (convenience macro).
/// Usage: SAGA_RPC_REGISTER(dispatch, "CastSpell", &OnCastSpell, true);
#define SAGA_RPC_REGISTER(dispatch, name, func, requiresAuth) \
    (dispatch).Register(name, func, requiresAuth)

/// Call an RPC from a client (legacy macro; prefer using RPCCodec directly).
/// Usage: SAGA_RPC_CALL(transport, clientId, rpcId, "CastSpell", arg1, arg2);
#define SAGA_RPC_CALL(transport, clientId, rpcId, name, ...) \
    do { \
        ::SagaServer::RPCRequest req; \
        req.rpcName = name; \
        req.clientId = clientId; \
        req.rpcId = rpcId; \
        /* Arguments must be encoded manually in production code; */ \
        /* this macro is kept for backwards compatibility only. */ \
    } while(0)

} // namespace SagaServer
