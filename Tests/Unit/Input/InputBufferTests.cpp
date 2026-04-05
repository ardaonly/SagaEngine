/// @file InputBufferTests.cpp
/// @brief Unit tests for InputCommandBuffer.

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#include "SagaEngine/Input/Commands/InputCommandBuffer.h"
#include "SagaEngine/Input/Commands/InputCommand.h"

using namespace SagaEngine::Input;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static InputCommand MakeCmd(uint32_t seq, uint32_t tick,
                             int32_t mx = 0, uint32_t buttons = 0)
{
    InputCommand cmd = MakeInputCommand(seq, tick, /*clientTimeUs=*/0);
    cmd.moveX   = mx;
    cmd.buttons = buttons;
    return cmd;
}

// ─── Fixture ─────────────────────────────────────────────────────────────────

class InputCommandBufferTest : public ::testing::Test
{
protected:
    InputCommandBuffer buf;
};

// ─── Basic push/peek ─────────────────────────────────────────────────────────

TEST_F(InputCommandBufferTest, EmptyByDefault)
{
    EXPECT_EQ(buf.GetPendingCount(),  0u);
    EXPECT_EQ(buf.GetUnackedCount(),  0u);
    EXPECT_FALSE(buf.IsFull());
}

TEST_F(InputCommandBufferTest, PushLocalIncreasesPending)
{
    EXPECT_TRUE(buf.PushLocal(MakeCmd(1, 10)));
    EXPECT_EQ(buf.GetPendingCount(), 1u);
    EXPECT_EQ(buf.GetUnackedCount(), 0u);
}

TEST_F(InputCommandBufferTest, PeekPendingReturnsCopy)
{
    buf.PushLocal(MakeCmd(1, 10));
    buf.PushLocal(MakeCmd(2, 11));
    const auto pending = buf.PeekPending();
    ASSERT_EQ(pending.size(), 2u);
    EXPECT_EQ(pending[0].sequence, 1u);
    EXPECT_EQ(pending[1].sequence, 2u);
    // Original buffer unchanged after peek.
    EXPECT_EQ(buf.GetPendingCount(), 2u);
}

// ─── MarkSent / unacked window ────────────────────────────────────────────────

TEST_F(InputCommandBufferTest, MarkSentMovesPendingToUnacked)
{
    buf.PushLocal(MakeCmd(1, 10));
    buf.PushLocal(MakeCmd(2, 11));
    buf.PushLocal(MakeCmd(3, 12));

    buf.MarkSent(/*upToSequence=*/2);

    EXPECT_EQ(buf.GetPendingCount(),  1u);  // cmd 3 stays pending
    EXPECT_EQ(buf.GetUnackedCount(),  2u);  // cmds 1+2 moved
}

TEST_F(InputCommandBufferTest, MarkSentHigherThanAllFlushesAll)
{
    buf.PushLocal(MakeCmd(1, 10));
    buf.PushLocal(MakeCmd(2, 11));
    buf.MarkSent(999);

    EXPECT_EQ(buf.GetPendingCount(), 0u);
    EXPECT_EQ(buf.GetUnackedCount(), 2u);
}

// ─── AckUpTo ─────────────────────────────────────────────────────────────────

TEST_F(InputCommandBufferTest, AckUpToRemovesUnacked)
{
    buf.PushLocal(MakeCmd(1, 10));
    buf.PushLocal(MakeCmd(2, 11));
    buf.PushLocal(MakeCmd(3, 12));
    buf.MarkSent(3);

    EXPECT_EQ(buf.GetUnackedCount(), 3u);

    buf.AckUpTo(2);
    EXPECT_EQ(buf.GetUnackedCount(), 1u);  // only cmd 3 remains

    const auto snap = buf.GetUnackedSnapshot();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].sequence, 3u);
}

TEST_F(InputCommandBufferTest, AckUpToAllClearsUnacked)
{
    buf.PushLocal(MakeCmd(5, 50));
    buf.PushLocal(MakeCmd(6, 51));
    buf.MarkSent(6);
    buf.AckUpTo(6);

    EXPECT_EQ(buf.GetUnackedCount(), 0u);
    EXPECT_EQ(buf.GetPendingCount(), 0u);
}

TEST_F(InputCommandBufferTest, AckUpToAlsoClearsPending)
{
    // Fast-path: ack without MarkSent (reconnect scenario).
    buf.PushLocal(MakeCmd(1, 10));
    buf.PushLocal(MakeCmd(2, 11));
    buf.AckUpTo(1);

    EXPECT_EQ(buf.GetPendingCount(), 1u);  // only cmd 2 remains
    EXPECT_EQ(buf.GetUnackedCount(), 0u);
}

// ─── GetUnackedSnapshot ───────────────────────────────────────────────────────

TEST_F(InputCommandBufferTest, UnackedSnapshotPreservesOrder)
{
    for (uint32_t i = 1; i <= 5; ++i)
        buf.PushLocal(MakeCmd(i, i * 10));
    buf.MarkSent(5);

    const auto snap = buf.GetUnackedSnapshot();
    ASSERT_EQ(snap.size(), 5u);
    for (uint32_t i = 0; i < 5; ++i)
        EXPECT_EQ(snap[i].sequence, i + 1);
}

// ─── FindByTick ──────────────────────────────────────────────────────────────

TEST_F(InputCommandBufferTest, FindByTickFindsInUnacked)
{
    buf.PushLocal(MakeCmd(1, 10));
    buf.PushLocal(MakeCmd(2, 20));
    buf.MarkSent(2);

    const auto found = buf.FindByTick(20);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->sequence, 2u);
}

TEST_F(InputCommandBufferTest, FindByTickFindsInPending)
{
    buf.PushLocal(MakeCmd(1, 10));
    buf.PushLocal(MakeCmd(2, 20));  // not yet sent

    const auto found = buf.FindByTick(20);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->sequence, 2u);
}

TEST_F(InputCommandBufferTest, FindByTickReturnsNulloptForMissing)
{
    buf.PushLocal(MakeCmd(1, 10));
    EXPECT_FALSE(buf.FindByTick(99).has_value());
}

// ─── Capacity / IsFull ────────────────────────────────────────────────────────

TEST_F(InputCommandBufferTest, RejectsWhenFull)
{
    // Fill to capacity.
    for (uint32_t i = 1; i <= kMaxUnackedCommands; ++i)
        buf.PushLocal(MakeCmd(i, i));

    EXPECT_TRUE(buf.IsFull());

    // One more push should fail.
    const bool ok = buf.PushLocal(MakeCmd(kMaxUnackedCommands + 1, 999));
    EXPECT_FALSE(ok);
}

TEST_F(InputCommandBufferTest, AckingMakesRoomForNewCommands)
{
    for (uint32_t i = 1; i <= kMaxUnackedCommands; ++i)
        buf.PushLocal(MakeCmd(i, i));

    buf.MarkSent(kMaxUnackedCommands);
    buf.AckUpTo(kMaxUnackedCommands);

    EXPECT_FALSE(buf.IsFull());
    EXPECT_TRUE(buf.PushLocal(MakeCmd(kMaxUnackedCommands + 1, 999)));
}

// ─── Reset ───────────────────────────────────────────────────────────────────

TEST_F(InputCommandBufferTest, ResetClearsEverything)
{
    buf.PushLocal(MakeCmd(1, 10));
    buf.PushLocal(MakeCmd(2, 11));
    buf.MarkSent(2);
    buf.Reset();

    EXPECT_EQ(buf.GetPendingCount(), 0u);
    EXPECT_EQ(buf.GetUnackedCount(), 0u);
    EXPECT_FALSE(buf.IsFull());
}

// ─── Thread safety ───────────────────────────────────────────────────────────

TEST_F(InputCommandBufferTest, ConcurrentPushAndAck)
{
    constexpr int kTotal = 1000;
    std::atomic<int> pushed{0};

    // Writer: push commands from a background thread.
    std::thread writer([&]
    {
        for (int i = 1; i <= kTotal; ++i)
        {
            if (buf.PushLocal(MakeCmd(static_cast<uint32_t>(i),
                                      static_cast<uint32_t>(i))))
                ++pushed;
            std::this_thread::yield();
        }
    });

    // Reader/acker: simulate network + ack from another thread.
    std::thread reader([&]
    {
        uint32_t lastAcked = 0;
        for (int iter = 0; iter < kTotal * 2; ++iter)
        {
            auto pending = buf.PeekPending();
            if (!pending.empty())
            {
                const uint32_t maxSeq = pending.back().sequence;
                buf.MarkSent(maxSeq);
                buf.AckUpTo(maxSeq);
                lastAcked = maxSeq;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    writer.join();
    reader.join();

    // No crash. Buffer in consistent state.
    EXPECT_GE(pushed.load(), 0);
    EXPECT_LE(buf.GetPendingCount() + buf.GetUnackedCount(),
              kMaxUnackedCommands);
}