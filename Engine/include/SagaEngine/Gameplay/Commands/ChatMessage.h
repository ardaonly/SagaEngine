/// @file ChatMessage.h
/// @brief Player-originated chat message on a specific channel.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: The server handles anti-spam, profanity filtering, channel
///          permissions and fan-out. Moderation and full text storage live
///          outside the command layer; this struct is the wire payload only.
///
/// Schema v1:
///   header(4) | channel(1) | recipientEntity(4) | clientMsgId(4)
///           | textLen(2) | text(N, UTF-8, max 512 bytes)

#pragma once

#include "SagaEngine/Gameplay/Commands/CommandCodec.h"

#include <cstdint>
#include <string>

namespace SagaEngine::Gameplay::Commands
{

// ─── ChatMessage ──────────────────────────────────────────────────────

/// Channel the message is being sent on.
enum class ChatChannel : std::uint8_t
{
    Say     = 0,   ///< Local proximity chat.
    Yell    = 1,   ///< Larger proximity chat.
    Whisper = 2,   ///< Private DM; recipientEntity required.
    Party   = 3,
    Guild   = 4,
    Raid    = 5,
    Zone    = 6,
    World   = 7,   ///< Server-wide (usually privileged).
    Custom  = 8,   ///< User-created custom channel; contextId carries channel id.
};

/// Maximum chat text length in bytes (UTF-8, including multibyte chars).
inline constexpr std::uint16_t kMaxChatTextBytes = 512;

/// Schema v1 payload for OpCode::ChatMessage.
struct ChatMessage
{
    static constexpr OpCode        kOpCode        = OpCode::ChatMessage;
    static constexpr std::uint16_t kSchemaVersion = 1;

    ChatChannel   channel         = ChatChannel::Say;
    std::uint32_t recipientEntity = 0;   ///< Whisper target or custom channel id.
    std::uint32_t clientMsgId     = 0;   ///< For client-side echo / dedupe.
    std::string   text;                  ///< UTF-8, length-prefixed on wire.

    void Encode(ByteWriter& w) const
    {
        w.WriteHeader(kOpCode, kSchemaVersion);
        w.WriteU8(static_cast<std::uint8_t>(channel));
        w.WriteU32(recipientEntity);
        w.WriteU32(clientMsgId);
        w.WriteString(text, kMaxChatTextBytes);
    }

    [[nodiscard]] static bool Decode(ByteReader& r, ChatMessage& out)
    {
        std::uint16_t version = 0;
        if (!r.ReadHeader(kOpCode, version)) return false;
        if (version != kSchemaVersion)       return false;

        out.channel         = static_cast<ChatChannel>(r.ReadU8());
        out.recipientEntity = r.ReadU32();
        out.clientMsgId     = r.ReadU32();
        out.text            = r.ReadString(kMaxChatTextBytes);
        return r.Ok();
    }
};

} // namespace SagaEngine::Gameplay::Commands
