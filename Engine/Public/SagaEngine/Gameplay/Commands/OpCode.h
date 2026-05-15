/// @file OpCode.h
/// @brief Numeric opcode enum and common header for gameplay commands.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: Gameplay commands are transported as a single RPC payload blob
///          (RPC name = "gcmd"). The first four bytes of every command are
///          this header; everything after is command-specific.
///
/// Design rules:
///   - OpCode discriminates the command type (stable across versions).
///   - schemaVersion is per-command (CastSpell v1, v2, ...), not global.
///   - New fields MUST bump the command's schemaVersion; handlers MUST
///     reject unknown versions rather than silently mis-decoding.
///   - Never reuse an OpCode value for a different command, even after
///     removal. Append-only.

#pragma once

#include <cstddef>
#include <cstdint>

namespace SagaEngine::Gameplay::Commands
{

// ─── OpCode ───────────────────────────────────────────────────────────

/// Stable numeric identifier for each gameplay command type.
/// Values are append-only; deprecated codes stay reserved forever.
enum class OpCode : std::uint16_t
{
    None         = 0,
    MoveRequest  = 1,   ///< Teleport / follow / path-intent (NOT tick input).
    CastSpell    = 2,
    Interact     = 3,
    Trade        = 4,
    UseItem      = 5,
    Equip        = 6,
    ChatMessage  = 7,

    // Append new entries below this line; never reorder.
};

// ─── Common header ────────────────────────────────────────────────────

/// Mandatory prefix serialised at the start of every gameplay command blob.
/// Serialisation is field-by-field little-endian; see CommandCodec.h.
struct GameplayCommandHeader
{
    std::uint16_t opCode        = 0;  ///< OpCode cast to integer.
    std::uint16_t schemaVersion = 0;  ///< Per-command schema revision.
};

/// Wire size of GameplayCommandHeader in bytes.
inline constexpr std::size_t kGameplayCommandHeaderSize = 4;

/// RPC name under which all gameplay commands are routed through the
/// RPC dispatch table. One RPC entry fans out to every opcode.
inline constexpr const char* kGameplayCommandRpcName = "gcmd";

/// Maximum serialised payload size (including header) for any gameplay
/// command. Bounded well below kMaxRPCPayloadBytes so the RPC layer can
/// still carry auxiliary arguments if ever needed.
inline constexpr std::size_t kMaxGameplayCommandPayload = 2048;

} // namespace SagaEngine::Gameplay::Commands
