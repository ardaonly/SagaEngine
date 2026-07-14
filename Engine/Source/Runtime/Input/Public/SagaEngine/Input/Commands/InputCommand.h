#pragma once

/// @file InputCommand.h
/// @brief Network-serializable input command: pure data, no logic.
///
/// Layer  : Input / Commands (Network boundary)
/// Purpose: Represents one client input event destined for the server.
///          This is the data that crosses the network. It is produced
///          from a GameplayInputFrame on the client and consumed by
///          the server's simulation tick.
///
/// Schema design — fixed vs dynamic:
///   We use a FIXED schema for MMO. Reasons:
///   - Deterministic bit layout: prediction and rollback rely on it.
///   - No allocation on the hot path: struct fits in a cache line.
///   - Simple serialization: memcpy + endian swap, no runtime dispatch.
///   - Easy to version: bump kCurrentVersion when fields change.
///   If the game later needs extensible commands (ability IDs, item use),
///   add an optional payload block after the fixed header.
///
/// Determinism note:
///   moveX/Y and lookX/Y are Q16.16 fixed-point integers (as int32_t).
///   Floating point is NEVER used in simulation-visible command fields.
///   Use FixedFromFloat() / FloatFromFixed() helpers for conversion.
///
/// Serialized wire size: 32 bytes (no padding, network byte order).

#include <cstdint>
#include <cmath>

namespace SagaEngine::Input
{

// Fixed-point helpers
/// Q16.16 fixed point: 16 bits integer part, 16 bits fractional.
/// Range: [-32768.0, 32767.99998]. Precision: ~0.000015.

inline constexpr int32_t kFixedScale = 65536;  // 1 << 16

[[nodiscard]] constexpr int32_t FixedFromFloat(float v) noexcept
{
    return static_cast<int32_t>(v * static_cast<float>(kFixedScale));
}

[[nodiscard]] constexpr float FloatFromFixed(int32_t v) noexcept
{
    return static_cast<float>(v) / static_cast<float>(kFixedScale);
}

// Button bit positions
/// Bits in InputCommand::buttons. Stable — never reorder.

enum InputCommandButtons : uint32_t
{
    kButtonFire      = 1u << 0,
    kButtonAltFire   = 1u << 1,
    kButtonJump      = 1u << 2,
    kButtonCrouch    = 1u << 3,
    kButtonInteract  = 1u << 4,
    kButtonSprint    = 1u << 5,
    kButtonReload    = 1u << 6,
    kButtonDodge     = 1u << 7,
    // Bits 8-31: reserved for project-specific actions
};

// Command struct

#pragma pack(push, 1)
struct InputCommand
{
    static constexpr uint8_t kCurrentVersion = 1;

    uint8_t  version;           ///< Schema version — always kCurrentVersion

    // Sequencing
    uint32_t sequence;          ///< Monotonic client-side counter (per connection)
    uint32_t simulationTick;    ///< Server tick this command targets

    // Timing
    uint64_t clientTimeUs;      ///< Client local time (latency analysis, not simulation)

    // Digital actions
    uint32_t buttons;           ///< Bitmask: see InputCommandButtons

    // Analog movement — Q16.16 fixed-point
    int32_t  moveX;             ///< Normalized [-1, 1] → FixedFromFloat
    int32_t  moveY;
    int32_t  lookX;             ///< Mouse / right stick delta
    int32_t  lookY;

    // Flags
    uint8_t  flags;             ///< Misc boolean state (bit 0: isSprinting, etc.)

    // Wire size: 1+4+4+8+4+4+4+4+4+1 = 38 bytes.
    // Round to 40 with 2 bytes reserved padding.
    uint8_t  _reserved[2]{};
};
#pragma pack(pop)

static_assert(sizeof(InputCommand) == 40,
    "InputCommand wire size changed — update kCurrentVersion and serializer");

// Builders

/// Zero-initialize with correct version and provided sequence/tick.
[[nodiscard]] inline InputCommand MakeInputCommand(
    uint32_t sequence,
    uint32_t tick,
    uint64_t clientTimeUs) noexcept
{
    InputCommand cmd{};
    cmd.version      = InputCommand::kCurrentVersion;
    cmd.sequence     = sequence;
    cmd.simulationTick = tick;
    cmd.clientTimeUs = clientTimeUs;
    return cmd;
}

[[nodiscard]] inline bool HasButton(const InputCommand& cmd, InputCommandButtons btn) noexcept
{
    return (cmd.buttons & static_cast<uint32_t>(btn)) != 0;
}

inline void SetButton(InputCommand& cmd, InputCommandButtons btn, bool active) noexcept
{
    if (active) cmd.buttons |=  static_cast<uint32_t>(btn);
    else        cmd.buttons &= ~static_cast<uint32_t>(btn);
}

} // namespace SagaEngine::Input