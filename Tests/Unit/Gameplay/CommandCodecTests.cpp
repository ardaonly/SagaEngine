/// @file CommandCodecTests.cpp
/// @brief Unit tests for the gameplay command codec and every command round-trip.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: Exercises the field-by-field ByteWriter / ByteReader primitives,
///          verifies wire-format determinism and endianness, and round-trips
///          every registered command. Covers failure modes (short buffer,
///          wrong opcode, wrong schema version, string overrun) so silent
///          mis-decodes stay impossible.

#include "SagaEngine/Gameplay/Commands/CastSpell.h"
#include "SagaEngine/Gameplay/Commands/ChatMessage.h"
#include "SagaEngine/Gameplay/Commands/CommandCodec.h"
#include "SagaEngine/Gameplay/Commands/CommandTraits.h"
#include "SagaEngine/Gameplay/Commands/Equip.h"
#include "SagaEngine/Gameplay/Commands/Interact.h"
#include "SagaEngine/Gameplay/Commands/MoveRequest.h"
#include "SagaEngine/Gameplay/Commands/OpCode.h"
#include "SagaEngine/Gameplay/Commands/Trade.h"
#include "SagaEngine/Gameplay/Commands/UseItem.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace SagaEngine::Gameplay::Commands;

namespace
{

template <typename CommandT>
std::vector<std::uint8_t> Encode(const CommandT& cmd)
{
    std::vector<std::uint8_t> buf;
    ByteWriter w(buf);
    cmd.Encode(w);
    return buf;
}

} // namespace

// ─── ByteWriter / ByteReader primitives ───────────────────────────────

TEST(ByteCodec, RoundTripPrimitives)
{
    std::vector<std::uint8_t> buf;
    ByteWriter w(buf);
    w.WriteU8(0xAB);
    w.WriteU16(0xBEEF);
    w.WriteU32(0xDEADBEEF);
    w.WriteU64(0x0123456789ABCDEFull);
    w.WriteI32(-42);
    w.WriteBool(true);
    w.WriteFloat(3.1415927f);
    w.WriteDouble(2.718281828459045);
    w.WriteString("hello world");

    ByteReader r(buf.data(), buf.size());
    EXPECT_EQ(r.ReadU8(), 0xABu);
    EXPECT_EQ(r.ReadU16(), 0xBEEFu);
    EXPECT_EQ(r.ReadU32(), 0xDEADBEEFu);
    EXPECT_EQ(r.ReadU64(), 0x0123456789ABCDEFull);
    EXPECT_EQ(r.ReadI32(), -42);
    EXPECT_TRUE(r.ReadBool());
    EXPECT_FLOAT_EQ(r.ReadFloat(), 3.1415927f);
    EXPECT_DOUBLE_EQ(r.ReadDouble(), 2.718281828459045);
    EXPECT_EQ(r.ReadString(), "hello world");
    EXPECT_TRUE(r.Ok());
    EXPECT_EQ(r.Remaining(), 0u);
}

TEST(ByteCodec, LittleEndianByteOrderIsStable)
{
    std::vector<std::uint8_t> buf;
    ByteWriter w(buf);
    w.WriteU32(0x11223344);
    ASSERT_EQ(buf.size(), 4u);
    EXPECT_EQ(buf[0], 0x44u);
    EXPECT_EQ(buf[1], 0x33u);
    EXPECT_EQ(buf[2], 0x22u);
    EXPECT_EQ(buf[3], 0x11u);
}

TEST(ByteCodec, UnderflowFailsReaderNotProcess)
{
    std::vector<std::uint8_t> buf{0x01, 0x02};
    ByteReader r(buf.data(), buf.size());
    EXPECT_EQ(r.ReadU16(), 0x0201u);
    EXPECT_TRUE(r.Ok());
    // Next read has no bytes left — should fail the reader, not the process.
    (void)r.ReadU32();
    EXPECT_FALSE(r.Ok());
}

TEST(ByteCodec, StringOverLimitFailsReader)
{
    std::vector<std::uint8_t> buf;
    ByteWriter w(buf);
    w.WriteString(std::string(100, 'a'));
    ByteReader r(buf.data(), buf.size());
    auto s = r.ReadString(/*maxBytes*/ 10);
    EXPECT_FALSE(r.Ok());
    EXPECT_TRUE(s.empty());
}

TEST(ByteCodec, HeaderReadRejectsWrongOpcode)
{
    std::vector<std::uint8_t> buf;
    ByteWriter w(buf);
    w.WriteHeader(OpCode::CastSpell, 1);
    ByteReader r(buf.data(), buf.size());
    std::uint16_t version = 0;
    EXPECT_FALSE(r.ReadHeader(OpCode::MoveRequest, version));
    EXPECT_FALSE(r.Ok());
}

TEST(ByteCodec, PeekOpCodeDoesNotConsume)
{
    std::vector<std::uint8_t> buf;
    ByteWriter w(buf);
    w.WriteHeader(OpCode::ChatMessage, 7);
    OpCode op = OpCode::None;
    ASSERT_TRUE(ByteReader::PeekOpCode(buf.data(), buf.size(), op));
    EXPECT_EQ(op, OpCode::ChatMessage);
    // Buffer is unchanged — a normal reader still sees the header.
    ByteReader r(buf.data(), buf.size());
    std::uint16_t version = 0;
    EXPECT_TRUE(r.ReadHeader(OpCode::ChatMessage, version));
    EXPECT_EQ(version, 7u);
}

TEST(ByteCodec, PeekOpCodeRejectsShortBuffer)
{
    std::uint8_t scraps[2] = {0x01, 0x00};
    OpCode op = OpCode::None;
    EXPECT_FALSE(ByteReader::PeekOpCode(scraps, sizeof(scraps), op));
    EXPECT_FALSE(ByteReader::PeekOpCode(nullptr, 0, op));
}

// ─── Per-command round trips ──────────────────────────────────────────

TEST(CommandRoundTrip, MoveRequest)
{
    MoveRequest a{};
    a.kind         = MoveRequestKind::FollowEntity;
    a.targetEntity = 0xFEED0042;
    a.targetX      = -12.5f;
    a.targetY      =  3.25f;
    a.targetZ      =  99.0f;
    a.speedHint    = 7.5f;
    a.cancel       = true;

    auto buf = Encode(a);
    MoveRequest b{};
    ByteReader r(buf.data(), buf.size());
    ASSERT_TRUE(MoveRequest::Decode(r, b));

    EXPECT_EQ(b.kind, MoveRequestKind::FollowEntity);
    EXPECT_EQ(b.targetEntity, 0xFEED0042u);
    EXPECT_FLOAT_EQ(b.targetX, -12.5f);
    EXPECT_FLOAT_EQ(b.targetY,   3.25f);
    EXPECT_FLOAT_EQ(b.targetZ,  99.0f);
    EXPECT_FLOAT_EQ(b.speedHint, 7.5f);
    EXPECT_TRUE(b.cancel);
}

TEST(CommandRoundTrip, CastSpell)
{
    CastSpell a{};
    a.spellId           = 0x4242;
    a.targetEntity      = 7;
    a.targetX           = 1.5f;
    a.targetY           = -2.25f;
    a.targetZ           = 3.75f;
    a.clientCastStartMs = 0x1122334455667788ull;
    a.comboFlags        = 0xAB;

    auto buf = Encode(a);
    CastSpell b{};
    ByteReader r(buf.data(), buf.size());
    ASSERT_TRUE(CastSpell::Decode(r, b));

    EXPECT_EQ(b.spellId,           a.spellId);
    EXPECT_EQ(b.targetEntity,      a.targetEntity);
    EXPECT_FLOAT_EQ(b.targetX,     a.targetX);
    EXPECT_FLOAT_EQ(b.targetY,     a.targetY);
    EXPECT_FLOAT_EQ(b.targetZ,     a.targetZ);
    EXPECT_EQ(b.clientCastStartMs, a.clientCastStartMs);
    EXPECT_EQ(b.comboFlags,        a.comboFlags);
}

TEST(CommandRoundTrip, Interact)
{
    Interact a{};
    a.kind         = InteractKind::DialogueReply;
    a.targetEntity = 10;
    a.contextId    = 555;
    a.optionIndex  = 3;

    auto buf = Encode(a);
    Interact b{};
    ByteReader r(buf.data(), buf.size());
    ASSERT_TRUE(Interact::Decode(r, b));
    EXPECT_EQ(b.kind,         InteractKind::DialogueReply);
    EXPECT_EQ(b.targetEntity, 10u);
    EXPECT_EQ(b.contextId,    555u);
    EXPECT_EQ(b.optionIndex,  3u);
}

TEST(CommandRoundTrip, Trade)
{
    Trade a{};
    a.verb          = TradeVerb::PutItem;
    a.partnerEntity = 42;
    a.slotIndex     = 2;
    a.itemEntity    = 777;
    a.quantity      = 5;
    a.goldOffered   = 1'000'000ull;

    auto buf = Encode(a);
    Trade b{};
    ByteReader r(buf.data(), buf.size());
    ASSERT_TRUE(Trade::Decode(r, b));
    EXPECT_EQ(b.verb,          TradeVerb::PutItem);
    EXPECT_EQ(b.partnerEntity, 42u);
    EXPECT_EQ(b.slotIndex,     2u);
    EXPECT_EQ(b.itemEntity,    777u);
    EXPECT_EQ(b.quantity,      5u);
    EXPECT_EQ(b.goldOffered,   1'000'000ull);
}

TEST(CommandRoundTrip, UseItem)
{
    UseItem a{};
    a.itemEntity    = 11;
    a.inventorySlot = 17;
    a.targetEntity  = 0;
    a.targetX       = 1.0f;
    a.targetY       = 2.0f;
    a.targetZ       = 3.0f;
    a.chargeIndex   = 4;

    auto buf = Encode(a);
    UseItem b{};
    ByteReader r(buf.data(), buf.size());
    ASSERT_TRUE(UseItem::Decode(r, b));
    EXPECT_EQ(b.itemEntity,    a.itemEntity);
    EXPECT_EQ(b.inventorySlot, a.inventorySlot);
    EXPECT_FLOAT_EQ(b.targetX, a.targetX);
    EXPECT_EQ(b.chargeIndex,   a.chargeIndex);
}

TEST(CommandRoundTrip, Equip)
{
    Equip a{};
    a.verb          = EquipVerb::Swap;
    a.itemEntity    = 0xABCD;
    a.paperDollSlot = PaperDollSlot::MainHand;
    a.altSlot       = PaperDollSlot::OffHand;

    auto buf = Encode(a);
    Equip b{};
    ByteReader r(buf.data(), buf.size());
    ASSERT_TRUE(Equip::Decode(r, b));
    EXPECT_EQ(b.verb,          EquipVerb::Swap);
    EXPECT_EQ(b.itemEntity,    0xABCDu);
    EXPECT_EQ(b.paperDollSlot, PaperDollSlot::MainHand);
    EXPECT_EQ(b.altSlot,       PaperDollSlot::OffHand);
}

TEST(CommandRoundTrip, ChatMessage)
{
    ChatMessage a{};
    a.channel         = ChatChannel::Whisper;
    a.recipientEntity = 999;
    a.clientMsgId     = 123456789;
    a.text            = "merhaba dünya — unicode test 🎮";

    auto buf = Encode(a);
    ChatMessage b{};
    ByteReader r(buf.data(), buf.size());
    ASSERT_TRUE(ChatMessage::Decode(r, b));
    EXPECT_EQ(b.channel,         ChatChannel::Whisper);
    EXPECT_EQ(b.recipientEntity, 999u);
    EXPECT_EQ(b.clientMsgId,     123456789u);
    EXPECT_EQ(b.text,            a.text);
}

TEST(CommandRoundTrip, EmptyChatMessageIsValid)
{
    ChatMessage a{};
    auto buf = Encode(a);
    ChatMessage b{};
    ByteReader r(buf.data(), buf.size());
    ASSERT_TRUE(ChatMessage::Decode(r, b));
    EXPECT_TRUE(b.text.empty());
}

// ─── Failure modes ────────────────────────────────────────────────────

TEST(CommandFailure, TruncatedBufferFailsDecode)
{
    CastSpell a{};
    a.spellId = 7;
    auto buf = Encode(a);
    // Drop the last byte.
    buf.pop_back();

    CastSpell b{};
    ByteReader r(buf.data(), buf.size());
    EXPECT_FALSE(CastSpell::Decode(r, b));
}

TEST(CommandFailure, WrongOpcodeInHeaderFailsDecode)
{
    CastSpell a{};
    auto buf = Encode(a);
    // Corrupt OpCode in the header (first two bytes).
    buf[0] = static_cast<std::uint8_t>(OpCode::ChatMessage);

    CastSpell b{};
    ByteReader r(buf.data(), buf.size());
    EXPECT_FALSE(CastSpell::Decode(r, b));
}

TEST(CommandFailure, UnknownSchemaVersionFailsDecode)
{
    CastSpell a{};
    auto buf = Encode(a);
    // Corrupt schemaVersion (bytes 2..3) to an unsupported value.
    buf[2] = 0xFF;
    buf[3] = 0xFF;

    CastSpell b{};
    ByteReader r(buf.data(), buf.size());
    EXPECT_FALSE(CastSpell::Decode(r, b));
}

TEST(CommandFailure, ChatMessageOverlongTextFailsDecode)
{
    // Manually build a ChatMessage whose declared text length exceeds the cap.
    std::vector<std::uint8_t> buf;
    ByteWriter w(buf);
    w.WriteHeader(OpCode::ChatMessage, ChatMessage::kSchemaVersion);
    w.WriteU8(static_cast<std::uint8_t>(ChatChannel::Say));
    w.WriteU32(0);           // recipientEntity
    w.WriteU32(0);           // clientMsgId
    // Declare a text length larger than kMaxChatTextBytes even if we don't
    // actually append that many bytes — decoder must reject.
    w.WriteU16(static_cast<std::uint16_t>(kMaxChatTextBytes + 1));

    ChatMessage b{};
    ByteReader r(buf.data(), buf.size());
    EXPECT_FALSE(ChatMessage::Decode(r, b));
}

// ─── Traits ───────────────────────────────────────────────────────────

TEST(CommandTraitsTests, RequireAuthIsTrueForAllShippedCommands)
{
    EXPECT_TRUE(CommandTraits<OpCode::MoveRequest>::requiresAuth);
    EXPECT_TRUE(CommandTraits<OpCode::CastSpell>::requiresAuth);
    EXPECT_TRUE(CommandTraits<OpCode::Interact>::requiresAuth);
    EXPECT_TRUE(CommandTraits<OpCode::Trade>::requiresAuth);
    EXPECT_TRUE(CommandTraits<OpCode::UseItem>::requiresAuth);
    EXPECT_TRUE(CommandTraits<OpCode::Equip>::requiresAuth);
    EXPECT_TRUE(CommandTraits<OpCode::ChatMessage>::requiresAuth);
}

TEST(CommandTraitsTests, AllShippedCommandsAreReliable)
{
    EXPECT_EQ(CommandTraits<OpCode::MoveRequest>::reliability, Reliability::Reliable);
    EXPECT_EQ(CommandTraits<OpCode::CastSpell>::reliability,   Reliability::Reliable);
    EXPECT_EQ(CommandTraits<OpCode::Trade>::reliability,       Reliability::Reliable);
    EXPECT_EQ(CommandTraits<OpCode::ChatMessage>::reliability, Reliability::Reliable);
}

TEST(CommandTraitsTests, MakeTraitsViewMirrorsCompileTimeValues)
{
    constexpr auto v = MakeTraitsView<OpCode::CastSpell>();
    static_assert(v.opCode             == OpCode::CastSpell);
    static_assert(v.reliability        == Reliability::Reliable);
    static_assert(v.requiresAuth       == true);
    static_assert(v.rateLimitPerSecond == 10);
    EXPECT_STREQ(v.debugName, "CastSpell");
}
