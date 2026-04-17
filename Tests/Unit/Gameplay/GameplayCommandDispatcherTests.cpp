/// @file GameplayCommandDispatcherTests.cpp
/// @brief Unit tests for the server-side gameplay command dispatcher and the
///        client-side typed stub, verifying the RPC → typed-handler pipeline.
///
/// Layer  : SagaServer / Gameplay   +   SagaEngine / Client / Gameplay
/// Purpose: Covers:
///            - Registration + typed dispatch by OpCode,
///            - Auth gate when CommandTraits::requiresAuth is true,
///            - Per-(client, opcode) token-bucket rate limiting,
///            - Rejection of unknown opcodes and malformed blobs,
///            - Client stub encodes + populates SendContext from traits,
///            - Full encoder → dispatcher → typed handler round trip.

#include "SagaEngine/Client/Gameplay/GameplayCommandClient.h"
#include "SagaEngine/Gameplay/Commands/CastSpell.h"
#include "SagaEngine/Gameplay/Commands/ChatMessage.h"
#include "SagaEngine/Gameplay/Commands/MoveRequest.h"
#include "SagaServer/Gameplay/GameplayCommandDispatcher.h"
#include "SagaServer/Gameplay/Handlers/GameplayHandlers.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace EC = SagaEngine::Gameplay::Commands;
namespace SG = SagaServer::Gameplay;
namespace CG = SagaEngine::Client::Gameplay;

// ─── Dispatcher: registration & typed dispatch ────────────────────────

TEST(GameplayCommandDispatcher, RegisterHandlerBumpsCount)
{
    SG::GameplayCommandDispatcher d;
    EXPECT_EQ(d.RegisteredCount(), 0u);

    auto handler = [](std::uint64_t, const EC::CastSpell&,
                       std::vector<std::uint8_t>&) { return true; };
    EXPECT_TRUE(d.RegisterHandler<EC::CastSpell>(handler));
    EXPECT_EQ(d.RegisteredCount(), 1u);

    // Second register for the same opcode is refused.
    EXPECT_FALSE(d.RegisterHandler<EC::CastSpell>(handler));
}

TEST(GameplayCommandDispatcher, TypedHandlerReceivesDecodedCommand)
{
    SG::GameplayCommandDispatcher d;

    EC::CastSpell received{};
    std::atomic<int> calls{0};
    std::uint64_t   seenClient = 0;

    d.RegisterHandler<EC::CastSpell>(
        [&](std::uint64_t clientId, const EC::CastSpell& cmd,
            std::vector<std::uint8_t>&) {
            received   = cmd;
            seenClient = clientId;
            calls.fetch_add(1, std::memory_order_relaxed);
            return true;
        });

    EC::CastSpell sent{};
    sent.spellId           = 42;
    sent.targetEntity      = 1337;
    sent.clientCastStartMs = 9876543210ull;

    std::vector<std::uint8_t> buf;
    EC::ByteWriter w(buf);
    sent.Encode(w);

    std::vector<std::uint8_t> out;
    auto status = d.DispatchBlob(/*clientId*/42, /*auth*/true,
                                   buf.data(), buf.size(), out);
    EXPECT_EQ(status, SagaServer::RPCStatusCode::Ok);
    EXPECT_EQ(calls.load(), 1);
    EXPECT_EQ(seenClient, 42u);
    EXPECT_EQ(received.spellId,           sent.spellId);
    EXPECT_EQ(received.targetEntity,      sent.targetEntity);
    EXPECT_EQ(received.clientCastStartMs, sent.clientCastStartMs);
}

TEST(GameplayCommandDispatcher, UnknownOpcodeReturnsNotFound)
{
    SG::GameplayCommandDispatcher d;
    // Register only ChatMessage; dispatch CastSpell.
    d.RegisterHandler<EC::ChatMessage>(
        [](std::uint64_t, const EC::ChatMessage&,
           std::vector<std::uint8_t>&) { return true; });

    EC::CastSpell cmd{};
    std::vector<std::uint8_t> buf;
    EC::ByteWriter w(buf); cmd.Encode(w);

    std::vector<std::uint8_t> out;
    auto status = d.DispatchBlob(1, true, buf.data(), buf.size(), out);
    EXPECT_EQ(status, SagaServer::RPCStatusCode::NotFound);
}

TEST(GameplayCommandDispatcher, MalformedBlobReturnsBadArgs)
{
    SG::GameplayCommandDispatcher d;
    d.RegisterHandler<EC::CastSpell>(
        [](std::uint64_t, const EC::CastSpell&,
           std::vector<std::uint8_t>&) { return true; });

    std::uint8_t scraps[3] = {0x00, 0x00, 0x00};
    std::vector<std::uint8_t> out;
    EXPECT_EQ(d.DispatchBlob(1, true, scraps, sizeof(scraps), out),
              SagaServer::RPCStatusCode::BadArgs);
    EXPECT_EQ(d.DispatchBlob(1, true, nullptr, 0, out),
              SagaServer::RPCStatusCode::BadArgs);
}

TEST(GameplayCommandDispatcher, UnauthenticatedClientIsRejectedWhenTraitsRequireAuth)
{
    SG::GameplayCommandDispatcher d;
    d.RegisterHandler<EC::CastSpell>(
        [](std::uint64_t, const EC::CastSpell&,
           std::vector<std::uint8_t>&) { return true; });

    EC::CastSpell cmd{};
    std::vector<std::uint8_t> buf;
    EC::ByteWriter w(buf); cmd.Encode(w);

    std::vector<std::uint8_t> out;
    EXPECT_EQ(d.DispatchBlob(1, /*auth*/false,
                              buf.data(), buf.size(), out),
              SagaServer::RPCStatusCode::AuthFailed);
}

TEST(GameplayCommandDispatcher, PerClientPerOpcodeRateLimit)
{
    SG::GameplayCommandDispatcher d;
    // ChatMessage trait: 4 msgs/sec capacity.
    std::atomic<int> hits{0};
    d.RegisterHandler<EC::ChatMessage>(
        [&](std::uint64_t, const EC::ChatMessage&,
            std::vector<std::uint8_t>&) {
            hits.fetch_add(1, std::memory_order_relaxed);
            return true;
        });

    EC::ChatMessage cmd{};
    cmd.text = "spam";
    std::vector<std::uint8_t> buf;
    EC::ByteWriter w(buf); cmd.Encode(w);

    // Burst through the bucket quickly — capacity is 4.
    int ok = 0, limited = 0;
    std::vector<std::uint8_t> out;
    for (int i = 0; i < 20; ++i)
    {
        auto s = d.DispatchBlob(/*clientId*/99, true,
                                  buf.data(), buf.size(), out);
        if (s == SagaServer::RPCStatusCode::Ok)            ++ok;
        else if (s == SagaServer::RPCStatusCode::RateLimited) ++limited;
        else FAIL() << "unexpected status";
    }
    EXPECT_GE(ok, 1);
    EXPECT_LE(ok, 5);          // Capacity is 4; tiny refill during the loop may add 0-1.
    EXPECT_GT(limited, 0);
    EXPECT_EQ(ok + limited, 20);
}

TEST(GameplayCommandDispatcher, RateLimitIsPerClient)
{
    SG::GameplayCommandDispatcher d;
    d.RegisterHandler<EC::ChatMessage>(
        [](std::uint64_t, const EC::ChatMessage&,
           std::vector<std::uint8_t>&) { return true; });

    EC::ChatMessage cmd{};
    std::vector<std::uint8_t> buf;
    EC::ByteWriter w(buf); cmd.Encode(w);
    std::vector<std::uint8_t> out;

    // Drain client 1's bucket.
    SagaServer::RPCStatusCode last = SagaServer::RPCStatusCode::Ok;
    for (int i = 0; i < 10; ++i)
        last = d.DispatchBlob(/*clientId*/1, true, buf.data(), buf.size(), out);
    EXPECT_EQ(last, SagaServer::RPCStatusCode::RateLimited);

    // Client 2 must still get through.
    auto s = d.DispatchBlob(/*clientId*/2, true, buf.data(), buf.size(), out);
    EXPECT_EQ(s, SagaServer::RPCStatusCode::Ok);
}

TEST(GameplayCommandDispatcher, HandlerReturningFalseBecomesInternalError)
{
    SG::GameplayCommandDispatcher d;
    d.RegisterHandler<EC::Interact>(
        [](std::uint64_t, const EC::Interact&,
           std::vector<std::uint8_t>&) { return false; });

    EC::Interact cmd{};
    std::vector<std::uint8_t> buf;
    EC::ByteWriter w(buf); cmd.Encode(w);
    std::vector<std::uint8_t> out;

    EXPECT_EQ(d.DispatchBlob(1, true, buf.data(), buf.size(), out),
              SagaServer::RPCStatusCode::InternalError);
}

// ─── Bulk stub registration ───────────────────────────────────────────

TEST(GameplayCommandDispatcher, RegisterGameplayHandlerStubsCoversAllShippedOpcodes)
{
    SG::GameplayCommandDispatcher d;
    SG::Handlers::RegisterGameplayHandlerStubs(d);
    EXPECT_EQ(d.RegisteredCount(), 7u);
}

// ─── Client stub: SendContext + encoding ──────────────────────────────

TEST(GameplayCommandClient, SetsContextFromTraitsAndEncodesPayload)
{
    CG::SendContext capturedCtx{};
    std::vector<std::uint8_t> capturedPayload;
    bool sendOk = true;

    CG::GameplayCommandClient client(
        [&](const CG::SendContext& ctx,
            const std::uint8_t* data, std::size_t n) {
            capturedCtx = ctx;
            capturedPayload.assign(data, data + n);
            return sendOk;
        });

    EXPECT_TRUE(client.CastSpell(/*spellId*/7, /*target*/42));
    EXPECT_EQ(capturedCtx.opCode,       EC::OpCode::CastSpell);
    EXPECT_EQ(capturedCtx.reliability,  EC::Reliability::Reliable);
    EXPECT_TRUE(capturedCtx.requiresAuth);
    ASSERT_NE(capturedCtx.debugName, nullptr);
    EXPECT_STREQ(capturedCtx.debugName, "CastSpell");

    // Payload decodes cleanly back to the same values.
    EC::CastSpell decoded{};
    EC::ByteReader r(capturedPayload.data(), capturedPayload.size());
    ASSERT_TRUE(EC::CastSpell::Decode(r, decoded));
    EXPECT_EQ(decoded.spellId, 7u);
    EXPECT_EQ(decoded.targetEntity, 42u);
}

TEST(GameplayCommandClient, PropagatesSendFailure)
{
    CG::GameplayCommandClient client(
        [](const CG::SendContext&, const std::uint8_t*, std::size_t) {
            return false;
        });
    EXPECT_FALSE(client.MoveToPoint(1.0f, 2.0f, 3.0f));
}

TEST(GameplayCommandClient, MissingSendFnFails)
{
    CG::GameplayCommandClient client(nullptr);
    EXPECT_FALSE(client.InteractWith(10));
}

// ─── End-to-end: client stub → dispatcher → typed handler ─────────────

TEST(GameplayRoundTrip, ClientStubThroughDispatcherToTypedHandler)
{
    SG::GameplayCommandDispatcher dispatcher;

    EC::ChatMessage lastSeen{};
    std::atomic<int> calls{0};
    dispatcher.RegisterHandler<EC::ChatMessage>(
        [&](std::uint64_t, const EC::ChatMessage& cmd,
            std::vector<std::uint8_t>&) {
            lastSeen = cmd;
            calls.fetch_add(1, std::memory_order_relaxed);
            return true;
        });

    // Client side: the "transport" pipes straight into the dispatcher here,
    // emulating what the real RPC bridge will do in production.
    CG::GameplayCommandClient client(
        [&](const CG::SendContext& ctx,
            const std::uint8_t* data, std::size_t n) {
            EXPECT_EQ(ctx.opCode, EC::OpCode::ChatMessage);
            std::vector<std::uint8_t> out;
            auto s = dispatcher.DispatchBlob(/*clientId*/123, /*auth*/true,
                                               data, n, out);
            return s == SagaServer::RPCStatusCode::Ok;
        });

    EXPECT_TRUE(client.Say(EC::ChatChannel::Guild,
                            "hello guild",
                            /*recipient*/0,
                            /*clientMsgId*/7));
    EXPECT_EQ(calls.load(), 1);
    EXPECT_EQ(lastSeen.channel,     EC::ChatChannel::Guild);
    EXPECT_EQ(lastSeen.text,        "hello guild");
    EXPECT_EQ(lastSeen.clientMsgId, 7u);
}
