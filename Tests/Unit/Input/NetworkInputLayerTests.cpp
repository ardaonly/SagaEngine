/// @file NetworkInputLayerTests.cpp
/// @brief Unit tests for the input networking layer:
///        InputCommand, InputCommandSerializer, InputCommandInbox,
///        ServerInputProcessor, InputPacketHandler.

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <span>
#include <algorithm>

#include "SagaEngine/Input/Commands/InputCommand.h"
#include "SagaEngine/Input/Commands/InputCommandSerializer.h"
#include "SagaEngine/Input/Networking/InputCommandInbox.h"
#include "SagaEngine/Input/Networking/ServerInputProcessor.h"
#include "SagaEngine/Input/Networking/InputPacketHandler.h"
#include "SagaServer/Networking/Core/Packet.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"

using namespace SagaEngine::Input;
using namespace SagaEngine::Networking;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static InputCommand MakeCmd(uint32_t seq, uint32_t tick,
                             int32_t mx = 0, int32_t my = 0,
                             uint32_t buttons = 0)
{
    InputCommand cmd = MakeInputCommand(seq, tick, /*ts=*/0);
    cmd.moveX   = mx;
    cmd.moveY   = my;
    cmd.buttons = buttons;
    return cmd;
}

// ─── InputCommand helpers ─────────────────────────────────────────────────────

TEST(InputCommandTest, FixedPointRoundTrip)
{
    constexpr float kOriginal = 0.75f;
    const int32_t fixed = FixedFromFloat(kOriginal);
    const float   back  = FloatFromFixed(fixed);

    EXPECT_NEAR(back, kOriginal, 0.001f);
}

TEST(InputCommandTest, ButtonSetAndClear)
{
    InputCommand cmd = MakeCmd(1, 1);

    SetButton(cmd, kButtonJump, true);
    EXPECT_TRUE(HasButton(cmd, kButtonJump));
    EXPECT_FALSE(HasButton(cmd, kButtonFire));

    SetButton(cmd, kButtonJump, false);
    EXPECT_FALSE(HasButton(cmd, kButtonJump));
}

TEST(InputCommandTest, MakeInputCommandDefaults)
{
    const auto cmd = MakeInputCommand(42, 100, 9999);
    EXPECT_EQ(cmd.version,         InputCommand::kCurrentVersion);
    EXPECT_EQ(cmd.sequence,        42u);
    EXPECT_EQ(cmd.simulationTick,  100u);
    EXPECT_EQ(cmd.clientTimeUs,    9999ull);
    EXPECT_EQ(cmd.buttons,         0u);
    EXPECT_EQ(cmd.moveX,           0);
}

// ─── InputCommandSerializer ───────────────────────────────────────────────────

class InputCommandSerializerTest : public ::testing::Test
{
protected:
    InputCommand original;

    void SetUp() override
    {
        original         = MakeInputCommand(7, 42, 123456);
        original.buttons = kButtonFire | kButtonJump;
        original.moveX   = FixedFromFloat( 0.5f);
        original.moveY   = FixedFromFloat(-1.0f);
        original.lookX   = FixedFromFloat( 2.5f);
        original.lookY   = FixedFromFloat(-0.1f);
        original.flags   = 0xAB;
    }
};

TEST_F(InputCommandSerializerTest, SerializeDeserializeRoundTrip)
{
    const auto buf = InputCommandSerializer::Serialize(original);
    EXPECT_EQ(buf.size(), InputCommandSerializer::kWireSize);

    InputCommand result{};
    const auto rc = InputCommandSerializer::Deserialize(
        std::span<const uint8_t>(buf.data(), buf.size()), result);

    EXPECT_EQ(rc, InputCommandSerializer::DeserializeResult::Ok);
    EXPECT_EQ(result.version,        original.version);
    EXPECT_EQ(result.sequence,       original.sequence);
    EXPECT_EQ(result.simulationTick, original.simulationTick);
    EXPECT_EQ(result.clientTimeUs,   original.clientTimeUs);
    EXPECT_EQ(result.buttons,        original.buttons);
    EXPECT_EQ(result.moveX,          original.moveX);
    EXPECT_EQ(result.moveY,          original.moveY);
    EXPECT_EQ(result.lookX,          original.lookX);
    EXPECT_EQ(result.lookY,          original.lookY);
    EXPECT_EQ(result.flags,          original.flags);
}

TEST_F(InputCommandSerializerTest, SerializeIntoDest)
{
    std::vector<uint8_t> dest(InputCommandSerializer::kWireSize, 0xFF);
    const size_t written = InputCommandSerializer::SerializeInto(
        original, std::span<uint8_t>(dest.data(), dest.size()));

    EXPECT_EQ(written, InputCommandSerializer::kWireSize);

    InputCommand result{};
    InputCommandSerializer::Deserialize(
        std::span<const uint8_t>(dest.data(), dest.size()), result);
    EXPECT_EQ(result.sequence, original.sequence);
}

TEST_F(InputCommandSerializerTest, RejectsBufferTooSmall)
{
    const auto buf = InputCommandSerializer::Serialize(original);
    std::vector<uint8_t> small(buf.begin(), buf.begin() + 10);

    InputCommand result{};
    const auto rc = InputCommandSerializer::Deserialize(
        std::span<const uint8_t>(small.data(), small.size()), result);

    EXPECT_EQ(rc, InputCommandSerializer::DeserializeResult::BufferTooSmall);
}

TEST_F(InputCommandSerializerTest, RejectsVersionMismatch)
{
    auto buf = InputCommandSerializer::Serialize(original);
    buf[0] = 0xFF;  // corrupt version byte

    InputCommand result{};
    const auto rc = InputCommandSerializer::Deserialize(
        std::span<const uint8_t>(buf.data(), buf.size()), result);

    EXPECT_EQ(rc, InputCommandSerializer::DeserializeResult::VersionMismatch);
}

TEST_F(InputCommandSerializerTest, BatchRoundTrip)
{
    std::vector<InputCommand> cmds;
    for (uint32_t i = 1; i <= 5; ++i)
        cmds.push_back(MakeCmd(i, i * 10));

    std::vector<uint8_t> batchBuf(cmds.size() * InputCommandSerializer::kWireSize);
    const size_t written = InputCommandSerializer::SerializeBatch(
        std::span<const InputCommand>(cmds.data(), cmds.size()),
        std::span<uint8_t>(batchBuf.data(), batchBuf.size()));

    EXPECT_EQ(written, cmds.size() * InputCommandSerializer::kWireSize);

    std::vector<InputCommand> results(cmds.size());
    const size_t read = InputCommandSerializer::DeserializeBatch(
        std::span<const uint8_t>(batchBuf.data(), batchBuf.size()),
        std::span<InputCommand>(results.data(), results.size()));

    EXPECT_EQ(read, cmds.size());
    for (size_t i = 0; i < cmds.size(); ++i)
        EXPECT_EQ(results[i].sequence, cmds[i].sequence);
}

TEST_F(InputCommandSerializerTest, LittleEndianLayout)
{
    // sequence = 0x01020304 → bytes [4,3,2,1] in LE at offset 1.
    InputCommand cmd = MakeInputCommand(0x01020304, 0, 0);
    const auto buf = InputCommandSerializer::Serialize(cmd);

    EXPECT_EQ(buf[1], 0x04);
    EXPECT_EQ(buf[2], 0x03);
    EXPECT_EQ(buf[3], 0x02);
    EXPECT_EQ(buf[4], 0x01);
}

// ─── InputCommandInbox ────────────────────────────────────────────────────────

class InputCommandInboxTest : public ::testing::Test
{
protected:
    SagaEngine::Input::InputCommandInbox inbox{ 1 };
};

TEST_F(InputCommandInboxTest, AcceptsFirst)
{
    const auto r = inbox.PushReceived(MakeCmd(1, 10));
    EXPECT_EQ(r, SagaEngine::Input::InputCommandInbox::PushResult::Accepted);
    EXPECT_EQ(inbox.GetQueueDepth(), 1u);
}

TEST_F(InputCommandInboxTest, RejectsDuplicate)
{
    inbox.PushReceived(MakeCmd(1, 10));
    const auto r = inbox.PushReceived(MakeCmd(1, 10));
    EXPECT_EQ(r, SagaEngine::Input::InputCommandInbox::PushResult::Duplicate);
    EXPECT_EQ(inbox.GetQueueDepth(), 1u);
}

TEST_F(InputCommandInboxTest, SortsOutOfOrder)
{
    inbox.PushReceived(MakeCmd(3, 30));
    inbox.PushReceived(MakeCmd(1, 10));
    inbox.PushReceived(MakeCmd(2, 20));

    auto r1 = inbox.ConsumeForTick(10);
    ASSERT_TRUE(r1.command.has_value());
    EXPECT_EQ(r1.command->sequence, 1u);

    auto r2 = inbox.ConsumeForTick(20);
    EXPECT_EQ(r2.command->sequence, 2u);
}

TEST_F(InputCommandInboxTest, LateArrivalReturnsFallback)
{
    inbox.PushReceived(MakeCmd(1, 10));
    inbox.ConsumeForTick(10);

    const auto r = inbox.ConsumeForTick(11);
    EXPECT_TRUE(r.isLate);
    EXPECT_TRUE(r.usedFallback);
    ASSERT_TRUE(r.command.has_value());
    EXPECT_EQ(r.command->sequence, 1u);
}

TEST_F(InputCommandInboxTest, EmptyFirstTickNoFallback)
{
    const auto r = inbox.ConsumeForTick(1);
    EXPECT_TRUE(r.isLate);
    EXPECT_FALSE(r.usedFallback);
    EXPECT_FALSE(r.command.has_value());
}

TEST_F(InputCommandInboxTest, AckAdvances)
{
    inbox.PushReceived(MakeCmd(5, 50));
    EXPECT_EQ(inbox.GetLastProcessedSequence(), 0u);
    inbox.ConsumeForTick(50);
    EXPECT_EQ(inbox.GetLastProcessedSequence(), 5u);
}

TEST_F(InputCommandInboxTest, RejectsOutOfWindow)
{
    // Advance the window by consuming many commands.
    for (uint32_t i = 1; i <= SagaEngine::Input::kInboxCapacity + 5; ++i)
    {
        inbox.PushReceived(MakeCmd(i, i));
        inbox.ConsumeForTick(i);
    }
    // Sequence 1 is now outside the acceptance window.
    const auto r = inbox.PushReceived(MakeCmd(1, 1));
    EXPECT_EQ(r, SagaEngine::Input::InputCommandInbox::PushResult::OutOfWindow);
}

TEST_F(InputCommandInboxTest, ResetClearsAll)
{
    inbox.PushReceived(MakeCmd(1, 1));
    inbox.ConsumeForTick(1);
    inbox.Reset();

    EXPECT_EQ(inbox.GetQueueDepth(),            0u);
    EXPECT_EQ(inbox.GetLastProcessedSequence(), 0u);
    EXPECT_EQ(inbox.GetTotalReceived(),         0u);
}

// ─── ServerInputProcessor ─────────────────────────────────────────────────────

class ServerInputProcessorTest : public ::testing::Test
{
protected:
    SagaEngine::Input::ServerInputProcessor proc;

    void SetUp() override
    {
        proc.AddClient(1);
        proc.AddClient(2);
    }
};

TEST_F(ServerInputProcessorTest, ConnectsAndDisconnects)
{
    EXPECT_EQ(proc.GetConnectedClientCount(), 2u);
    EXPECT_NE(proc.GetInbox(1), nullptr);

    proc.RemoveClient(1);
    EXPECT_EQ(proc.GetConnectedClientCount(), 1u);
    EXPECT_EQ(proc.GetInbox(1), nullptr);
}

TEST_F(ServerInputProcessorTest, ProcessTickDeliversBothClients)
{
    proc.GetInbox(1)->PushReceived(MakeCmd(1, 10));
    proc.GetInbox(2)->PushReceived(MakeCmd(1, 10));

    const auto result = proc.ProcessTick(10);
    EXPECT_EQ(result.entries.size(), 2u);
}

TEST_F(ServerInputProcessorTest, SanitizesClampedMove)
{
    SagaEngine::Input::InputValidationConfig cfg;
    cfg.maxMoveValue = 1.0f;
    SagaEngine::Input::ServerInputProcessor p(cfg);
    p.AddClient(1);

    auto cmd = MakeCmd(1, 10, FixedFromFloat(5.f), FixedFromFloat(-9.f));
    p.GetInbox(1)->PushReceived(cmd);

    const auto result = p.ProcessTick(10);
    ASSERT_EQ(result.entries.size(), 1u);
    EXPECT_TRUE(result.entries[0].wasInvalid);
    EXPECT_EQ(result.entries[0].command.moveX,  FixedFromFloat(cfg.maxMoveValue));
    EXPECT_EQ(result.entries[0].command.moveY, -FixedFromFloat(cfg.maxMoveValue));
}

TEST_F(ServerInputProcessorTest, StripsForbiddenButtons)
{
    SagaEngine::Input::InputValidationConfig cfg;
    cfg.forbiddenButtonMask = 0xFFFF0000;
    SagaEngine::Input::ServerInputProcessor p(cfg);
    p.AddClient(1);

    auto cmd = MakeCmd(1, 10, 0, 0, 0xFFFF0001);
    p.GetInbox(1)->PushReceived(cmd);

    const auto result = p.ProcessTick(10);
    ASSERT_EQ(result.entries.size(), 1u);
    EXPECT_EQ(result.entries[0].command.buttons & 0xFFFF0000, 0u);
    EXPECT_EQ(result.entries[0].command.buttons & 0x00000001, 1u);
}

TEST_F(ServerInputProcessorTest, EmitsAckForRealCommand)
{
    proc.GetInbox(1)->PushReceived(MakeCmd(9, 20));
    const auto result = proc.ProcessTick(20);

    const bool hasAck = std::any_of(result.acks.begin(), result.acks.end(),
        [](const SagaEngine::Input::AckEntry& a)
        { return a.clientId == 1 && a.sequence == 9; });
    EXPECT_TRUE(hasAck);
}

TEST_F(ServerInputProcessorTest, NoAckForFallback)
{
    proc.GetInbox(1)->PushReceived(MakeCmd(1, 1));
    proc.ProcessTick(1);

    const auto result = proc.ProcessTick(2);
    const bool hasAck = std::any_of(result.acks.begin(), result.acks.end(),
        [](const SagaEngine::Input::AckEntry& a) { return a.clientId == 1; });
    EXPECT_FALSE(hasAck);
}

// ─── InputPacketHandler ───────────────────────────────────────────────────────

static Packet MakeInputPacket(const InputCommand& cmd)
{
    const auto bytes = InputCommandSerializer::Serialize(cmd);
    Packet p(PacketType::InputCommand);
    p.WriteBytes(bytes.data(), bytes.size());
    return p;
}

class InputPacketHandlerTest : public ::testing::Test
{
protected:
    SagaEngine::Input::ServerInputProcessor proc;
    SagaEngine::Input::InputPacketHandler handler{ proc };

    void SetUp() override
    {
        proc.AddClient(100);
    }
};

TEST_F(InputPacketHandlerTest, DecodesAndRoutes)
{
    const auto cmd    = MakeCmd(1, 10, FixedFromFloat(0.5f));
    const auto packet = MakeInputPacket(cmd);

    handler.OnPacketReceived(100, packet);

    EXPECT_EQ(proc.GetInbox(100)->GetQueueDepth(), 1u);
    EXPECT_EQ(handler.GetTotalPacketsReceived(), 1u);
    EXPECT_EQ(handler.GetTotalPacketsRejected(), 0u);
}

TEST_F(InputPacketHandlerTest, IgnoresNonInputPackets)
{
    Packet keepAlive(PacketType::KeepAlive);
    handler.OnPacketReceived(100, keepAlive);

    EXPECT_EQ(proc.GetInbox(100)->GetQueueDepth(), 0u);
    EXPECT_EQ(handler.GetTotalPacketsReceived(), 0u);
}

TEST_F(InputPacketHandlerTest, DropsPacketForUnknownClient)
{
    const auto cmd    = MakeCmd(1, 10);
    const auto packet = MakeInputPacket(cmd);

    handler.OnPacketReceived(999, packet);  // connection 999 not registered

    EXPECT_EQ(handler.GetTotalPacketsRejected(), 1u);
}

TEST_F(InputPacketHandlerTest, BuildsAckPackets)
{
    std::vector<SagaEngine::Input::AckEntry> acks = {
        { 100, 7 },
        { 200, 3 },
    };

    const auto packets = handler.BuildAckPackets(acks, /*serverTick=*/42);

    ASSERT_EQ(packets.size(), 2u);
    EXPECT_EQ(packets[0].connectionId, 100u);
    EXPECT_EQ(packets[1].connectionId, 200u);
    for (const auto& ap : packets)
    {
        EXPECT_EQ(ap.packet.GetType(), PacketType::InputAck);
        EXPECT_EQ(ap.packet.GetPayloadSize(),
                  sizeof(SagaEngine::Input::InputAckPayload));
    }
}

TEST_F(InputPacketHandlerTest, RawBytesRoundTrip)
{
    const auto cmd     = MakeCmd(5, 50);
    const auto packet  = MakeInputPacket(cmd);
    const auto raw     = packet.GetSerializedData();

    const bool ok = handler.OnRawBytesReceived(100, raw.data(), raw.size());
    EXPECT_TRUE(ok);
    EXPECT_EQ(proc.GetInbox(100)->GetQueueDepth(), 1u);
}

// ─── Full pipeline integration ────────────────────────────────────────────────

TEST(NetworkInputPipelineTest, EndToEndCommandFlow)
{
    SagaEngine::Input::ServerInputProcessor proc;
    SagaEngine::Input::InputPacketHandler   handler{ proc };
    proc.AddClient(1);

    InputCommand cmd = MakeInputCommand(1, 10, 500000);
    cmd.buttons = kButtonFire | kButtonSprint;
    cmd.moveX   = FixedFromFloat(0.7f);
    cmd.moveY   = FixedFromFloat(-0.3f);

    const auto packet = MakeInputPacket(cmd);
    handler.OnPacketReceived(1, packet);

    const auto result = proc.ProcessTick(10);

    ASSERT_EQ(result.entries.size(), 1u);
    EXPECT_TRUE(result.entries[0].hasCommand);
    EXPECT_FALSE(result.entries[0].wasInvalid);

    const InputCommand& recv = result.entries[0].command;
    EXPECT_EQ(recv.sequence, 1u);
    EXPECT_EQ(recv.simulationTick, 10u);
    EXPECT_TRUE(HasButton(recv, kButtonFire));
    EXPECT_TRUE(HasButton(recv, kButtonSprint));
    EXPECT_NEAR(FloatFromFixed(recv.moveX), 0.7f, 0.001f);
    EXPECT_NEAR(FloatFromFixed(recv.moveY), -0.3f, 0.001f);

    ASSERT_EQ(result.acks.size(), 1u);
    EXPECT_EQ(result.acks[0].clientId, 1u);
    EXPECT_EQ(result.acks[0].sequence, 1u);
}

// ─── Concurrent inbox stress test ─────────────────────────────────────────────

TEST(NetworkInputLayerStressTest, ConcurrentPushAndConsumeInbox)
{
    SagaEngine::Input::InputCommandInbox inbox{ 99 };
    constexpr int kCommands = 500;
    std::atomic<int> accepted{ 0 };

    std::thread writer([&]
    {
        for (int i = 1; i <= kCommands; ++i)
        {
            const auto r = inbox.PushReceived(
                MakeCmd(static_cast<uint32_t>(i), static_cast<uint32_t>(i)));
            if (r == SagaEngine::Input::InputCommandInbox::PushResult::Accepted)
                ++accepted;
        }
    });

    int consumed = 0;
    std::thread reader([&]
    {
        for (int tick = 1; tick <= kCommands; ++tick)
        {
            auto r = inbox.ConsumeForTick(static_cast<uint32_t>(tick));
            if (r.command.has_value() && !r.usedFallback) ++consumed;
            std::this_thread::sleep_for(std::chrono::microseconds(30));
        }
    });

    writer.join();
    reader.join();

    // No crash; queue depth is bounded.
    EXPECT_LE(inbox.GetQueueDepth(), SagaEngine::Input::kInboxCapacity);
    EXPECT_EQ(inbox.GetTotalReceived(), static_cast<uint32_t>(kCommands));
    EXPECT_LE(inbox.GetQueueDepth(), SagaEngine::Input::kInboxCapacity);
}