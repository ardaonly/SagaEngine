/// @file MoveRequest.h
/// @brief High-level movement intent (teleport / follow / path) — not tick input.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: Per-tick movement (WASD, look axes) stays on InputCommandQueue,
///          which is prediction/reconciliation friendly. MoveRequest is the
///          *intent* channel: "I want to walk to this point", "follow this
///          entity", "teleport to this waypoint". The server is fully
///          authoritative — client-side validation is advisory only.
///
/// Schema v1:
///   header(4) | kind(1) | target.entity(4) | target.x(4) | target.y(4)
///           | target.z(4) | speedHint(4) | cancel(1)

#pragma once

#include "SagaEngine/Gameplay/Commands/CommandCodec.h"

#include <cstdint>

namespace SagaEngine::Gameplay::Commands
{

// ─── MoveRequest ──────────────────────────────────────────────────────

/// Kind of high-level movement intent the client is sending.
enum class MoveRequestKind : std::uint8_t
{
    Stop         = 0,  ///< Clear any active path / follow target.
    MoveToPoint  = 1,  ///< Path to a world position.
    FollowEntity = 2,  ///< Follow a target entity (chase, pet, escort).
    InteractMove = 3,  ///< Move into interaction range of a target entity.
    Teleport     = 4,  ///< Server will gate heavily (GM / zone transfer).
};

/// Schema v1 payload for OpCode::MoveRequest.
struct MoveRequest
{
    static constexpr OpCode        kOpCode        = OpCode::MoveRequest;
    static constexpr std::uint16_t kSchemaVersion = 1;

    MoveRequestKind kind        = MoveRequestKind::Stop;
    std::uint32_t   targetEntity = 0;     ///< 0 when kind is MoveToPoint / Teleport / Stop.
    float           targetX     = 0.0f;
    float           targetY     = 0.0f;
    float           targetZ     = 0.0f;
    float           speedHint   = 0.0f;   ///< Advisory only; server clamps.
    bool            cancel      = false;  ///< If true, cancel any in-flight intent.

    void Encode(ByteWriter& w) const
    {
        w.WriteHeader(kOpCode, kSchemaVersion);
        w.WriteU8(static_cast<std::uint8_t>(kind));
        w.WriteU32(targetEntity);
        w.WriteFloat(targetX);
        w.WriteFloat(targetY);
        w.WriteFloat(targetZ);
        w.WriteFloat(speedHint);
        w.WriteBool(cancel);
    }

    [[nodiscard]] static bool Decode(ByteReader& r, MoveRequest& out)
    {
        std::uint16_t version = 0;
        if (!r.ReadHeader(kOpCode, version)) return false;
        if (version != kSchemaVersion)       return false;

        out.kind         = static_cast<MoveRequestKind>(r.ReadU8());
        out.targetEntity = r.ReadU32();
        out.targetX      = r.ReadFloat();
        out.targetY      = r.ReadFloat();
        out.targetZ      = r.ReadFloat();
        out.speedHint    = r.ReadFloat();
        out.cancel       = r.ReadBool();
        return r.Ok();
    }
};

} // namespace SagaEngine::Gameplay::Commands
