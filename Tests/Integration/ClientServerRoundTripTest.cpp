/// @file ClientServerRoundTripTest.cpp
/// @brief End-to-end roundtrip test: client input → server apply → snapshot → client reconcile.
///
/// Layer  : Integration Tests
/// Purpose: Verifies the full MMO client pipeline as a single coordinated test:
///   1. Server spawns test entities
///   2. Client connects and receives initial snapshot
///   3. Client sends input commands
///   4. Server applies input to entities
///   5. Server sends updated snapshot
///   6. Client receives, applies, and reconciles
///   7. Position convergence is verified

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/ECS/ComponentRegistry.h>
#include <SagaEngine/ECS/ComponentSerializerRegistry.h>
#include <SagaEngine/Input/Commands/InputCommand.h>
#include <SagaEngine/Input/Commands/InputCommandBuffer.h>
#include <SagaEngine/Simulation/WorldState.h>
#include <SagaEngine/Client/Prediction/ReconciliationBuffer.h>
#include <SagaEngine/Client/Replication/EcsSnapshotApply.h>
#include <SagaEngine/Client/Replication/SnapshotApplyPipeline.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace SagaEngine::Testing {

static constexpr const char* kTag = "RoundTripTest";

// ─── Test component definition ────────────────────────────────────────────────
// Mirrors the layout used by TestSnapshotServer and ClientHost.

struct alignas(8) TestTransformComponent
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
};

static constexpr uint32_t kTestTransformTypeId = 1001;
static constexpr float kMoveScale = 0.05f; // units per tick — matches server + client

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace {

/// Register test components (called once per test suite).
void RegisterTestComponents()
{
    auto& cr = ECS::ComponentRegistry::Instance();
    if (cr.IsRegistered<TestTransformComponent>())
        return;

    SAGA_REGISTER_COMPONENT(TestTransformComponent, kTestTransformTypeId);

    auto& csr = ECS::ComponentSerializerRegistry::Instance();
    csr.Register<TestTransformComponent>(
        kTestTransformTypeId,
        "TestTransform",
        [](const TestTransformComponent& d, void* buf, size_t sz) -> size_t {
            if (sz < sizeof(TestTransformComponent)) return 0;
            std::memcpy(buf, &d, sizeof(TestTransformComponent));
            return sizeof(TestTransformComponent);
        },
        [](TestTransformComponent& d, const void* buf, size_t sz) -> size_t {
            if (sz < sizeof(TestTransformComponent)) return 0;
            std::memcpy(&d, buf, sizeof(TestTransformComponent));
            return sizeof(TestTransformComponent);
        },
        []() -> size_t { return sizeof(TestTransformComponent); });

    // Register authority: server-owned.
    Client::Replication::g_AuthorityTable.Register(
        kTestTransformTypeId, Client::Replication::ReplicationAuthority::ServerOwned);
}

/// Server-side: apply an input command to a test entity's transform.
void ServerApplyInput(TestTransformComponent& tc, const Input::InputCommand& cmd)
{
    const float moveX = Input::FloatFromFixed(cmd.moveX);
    const float moveY = Input::FloatFromFixed(cmd.moveY);
    tc.x += moveX * kMoveScale;
    tc.y += moveY * kMoveScale;
}

/// Client-side replay function: integrate one input command into predicted state.
void ClientReplayInput(
    std::uint32_t            inputSeq,
    float                    dtSeconds,
    Input::InputCommandBuffer& inputBuffer,
    Client::Prediction::PredictedState& stateOut)
{
    // Look up the command by sequence number in the unacked window.
    const Input::InputCommand* cmdPtr = nullptr;
    auto unacked = inputBuffer.GetUnackedSnapshot();
    for (const auto& c : unacked)
    {
        if (c.sequence == inputSeq)
        {
            cmdPtr = &c;
            break;
        }
    }

    if (!cmdPtr)
    {
        stateOut.position += stateOut.velocity * dtSeconds;
        return;
    }

    const float moveX = Input::FloatFromFixed(cmdPtr->moveX);
    const float moveY = Input::FloatFromFixed(cmdPtr->moveY);

    stateOut.position.x += moveX * kMoveScale;
    stateOut.position.z += moveY * kMoveScale;
    stateOut.velocity.x = moveX * kMoveScale / dtSeconds;
    stateOut.velocity.z = moveY * kMoveScale / dtSeconds;
}

} // anonymous namespace

// ─── Test fixture ─────────────────────────────────────────────────────────────

class ClientServerRoundTripTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterTestComponents();

        m_world = std::make_unique<Simulation::WorldState>();

        // Spawn a single test entity at the origin.
        m_entityHandle = m_world->CreateEntity();
        TestTransformComponent tc{};
        m_world->AddComponent<TestTransformComponent>(m_entityHandle.id, tc);
    }

    void TearDown() override
    {
        ECS::ComponentRegistry::Instance().Reset();
        ECS::ComponentSerializerRegistry::Instance().Reset();
        Client::Replication::g_AuthorityTable.Reset();
    }

    std::unique_ptr<Simulation::WorldState> m_world;
    ECS::EntityHandle m_entityHandle;
};

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_F(ClientServerRoundTripTest, InputMovesEntityOnServer)
{
    // Verify the entity starts at the origin.
    auto* tc = m_world->GetComponent<TestTransformComponent>(m_entityHandle.id);
    ASSERT_NE(tc, nullptr);
    EXPECT_FLOAT_EQ(tc->x, 0.0f);
    EXPECT_FLOAT_EQ(tc->y, 0.0f);

    // Simulate sending 10 input commands (move +X).
    for (uint32_t seq = 1; seq <= 10; ++seq)
    {
        Input::InputCommand cmd = Input::MakeInputCommand(
            seq, seq, seq * 16666); // 60 Hz timing
        cmd.moveX = Input::FixedFromFloat(1.0f); // full speed +X
        cmd.moveY = Input::FixedFromFloat(0.0f);

        ServerApplyInput(*tc, cmd);
    }

    // Entity should have moved: x = 10 * 1.0 * 0.05 = 0.5
    EXPECT_NEAR(tc->x, 0.5f, 0.001f);
    EXPECT_FLOAT_EQ(tc->y, 0.0f);
}

TEST_F(ClientServerRoundTripTest, PredictionReplayMatchesServer)
{
    Input::InputCommandBuffer inputBuffer;
    Client::Prediction::ReconciliationBuffer<128> reconciliation;

    const float dtSeconds = 1.0f / 60.0f;
    uint32_t inputSeq = 0;

    // Simulate 10 ticks of client prediction.
    for (int tick = 0; tick < 10; ++tick)
    {
        inputSeq++;
        Input::InputCommand cmd = Input::MakeInputCommand(
            inputSeq, static_cast<uint32_t>(tick), static_cast<uint64_t>(tick * 16666));
        cmd.moveX = Input::FixedFromFloat(1.0f);
        cmd.moveY = Input::FixedFromFloat(0.0f);

        // Push to input buffer (simulates input system producing the command).
        inputBuffer.PushLocal(cmd);

        // Record prediction: apply the same movement the server will apply.
        Client::Prediction::PredictedState pred;
        pred.inputSeq = inputSeq;
        pred.position = tick > 0 ? reconciliation.Find(inputSeq - 1)->position
                                  : Math::Vec3(0, 0, 0);
        pred.velocity = Math::Vec3(0, 0, 0);

        // Integrate this command into the prediction.
        pred.position.x += Input::FloatFromFixed(cmd.moveX) * kMoveScale;
        pred.position.z += Input::FloatFromFixed(cmd.moveY) * kMoveScale;

        reconciliation.Record(pred);

        // Also apply to the server entity (same logic).
        auto* tc = m_world->GetComponent<TestTransformComponent>(m_entityHandle.id);
        ServerApplyInput(*tc, cmd);
    }

    // Server state: x = 10 * 1.0 * 0.05 = 0.5
    auto* serverTc = m_world->GetComponent<TestTransformComponent>(m_entityHandle.id);
    EXPECT_NEAR(serverTc->x, 0.5f, 0.001f);

    // Client prediction: last recorded state should also be ~0.5 on x.
    auto lastPred = reconciliation.Find(inputSeq);
    ASSERT_TRUE(lastPred.has_value());
    EXPECT_NEAR(lastPred->position.x, 0.5f, 0.001f);
}

TEST_F(ClientServerRoundTripTest, ReconciliationCorrectsDivergence)
{
    Input::InputCommandBuffer inputBuffer;
    Client::Prediction::ReconciliationBuffer<128> reconciliation;

    const float dtSeconds = 1.0f / 60.0f;
    const float errorThresholdSq = 0.04f; // 20 cm
    uint32_t inputSeq = 0;

    // Simulate 5 ticks of normal prediction.
    for (int tick = 0; tick < 5; ++tick)
    {
        inputSeq++;
        Input::InputCommand cmd = Input::MakeInputCommand(
            inputSeq, static_cast<uint32_t>(tick), static_cast<uint64_t>(tick * 16666));
        cmd.moveX = Input::FixedFromFloat(1.0f);
        cmd.moveY = Input::FixedFromFloat(0.0f);

        inputBuffer.PushLocal(cmd);

        Client::Prediction::PredictedState pred;
        pred.inputSeq = inputSeq;
        pred.position = tick > 0 ? reconciliation.Find(inputSeq - 1)->position
                                  : Math::Vec3(0, 0, 0);
        pred.velocity = Math::Vec3(0, 0, 0);
        pred.position.x += Input::FloatFromFixed(cmd.moveX) * kMoveScale;

        reconciliation.Record(pred);
    }

    // Now simulate a server correction: the server says the player is at (0.1, 0, 0)
    // instead of the predicted (0.25, 0, 0). This could happen due to server-side
    // collision correction, speed clamp, etc.
    Client::Prediction::AuthoritativeState auth;
    auth.ackedInputSeq = 3; // server acked up to seq 3
    auth.position = Math::Vec3(0.1f, 0.0f, 0.0f); // corrected position
    auth.velocity = Math::Vec3(0, 0, 0);
    auth.rotation = Math::Quat::Identity();

    auto replayFn = [&inputBuffer, dtSeconds](
        std::uint32_t seq, float dt, Client::Prediction::PredictedState& out) {
        ClientReplayInput(seq, dt, inputBuffer, out);
    };

    auto result = reconciliation.AcknowledgeAndReconcile(
        auth, dtSeconds, errorThresholdSq, replayFn);

    // After reconciliation, the position should be close to the server's
    // corrected value plus replay of pending inputs (seq 4 and 5).
    // Pending inputs each add 0.05 to x, so expected x ≈ 0.1 + 2*0.05 = 0.2
    EXPECT_NEAR(result.position.x, 0.2f, 0.01f);
}

TEST_F(ClientServerRoundTripTest, FullLoopInputSnapshotReconcile)
{
    // This test simulates the full end-to-end loop without actual networking:
    // 1. Client predicts 10 ticks of movement
    // 2. Server applies the same 10 inputs
    // 3. Server "sends" snapshot with authoritative state
    // 4. Client receives, reconciles, and verifies convergence

    Input::InputCommandBuffer inputBuffer;
    Client::Prediction::ReconciliationBuffer<128> reconciliation;

    const float dtSeconds = 1.0f / 60.0f;
    const float errorThresholdSq = 0.04f;
    uint32_t inputSeq = 0;

    // Phase 1: Client predicts 10 ticks.
    for (int tick = 0; tick < 10; ++tick)
    {
        inputSeq++;
        Input::InputCommand cmd = Input::MakeInputCommand(
            inputSeq, static_cast<uint32_t>(tick), static_cast<uint64_t>(tick * 16666));
        cmd.moveX = Input::FixedFromFloat(1.0f);
        cmd.moveY = Input::FixedFromFloat(0.5f);

        inputBuffer.PushLocal(cmd);

        // Apply to server entity (simulating server receiving and applying input).
        auto* tc = m_world->GetComponent<TestTransformComponent>(m_entityHandle.id);
        ServerApplyInput(*tc, cmd);

        // Client prediction (same logic).
        Client::Prediction::PredictedState pred;
        pred.inputSeq = inputSeq;
        pred.position = tick > 0 ? reconciliation.Find(inputSeq - 1)->position
                                  : Math::Vec3(0, 0, 0);
        pred.velocity = Math::Vec3(0, 0, 0);
        pred.position.x += Input::FloatFromFixed(cmd.moveX) * kMoveScale;
        pred.position.z += Input::FloatFromFixed(cmd.moveY) * kMoveScale;
        pred.velocity.x = Input::FloatFromFixed(cmd.moveX) * kMoveScale / dtSeconds;
        pred.velocity.z = Input::FloatFromFixed(cmd.moveY) * kMoveScale / dtSeconds;

        reconciliation.Record(pred);
    }

    // Phase 2: Server sends snapshot (we read directly from the world).
    auto* serverTc = m_world->GetComponent<TestTransformComponent>(m_entityHandle.id);
    Math::Vec3 serverPos(serverTc->x, serverTc->y, serverTc->z);

    // Phase 3: Client receives snapshot and reconciles.
    Client::Prediction::AuthoritativeState auth;
    auth.ackedInputSeq = inputSeq; // server acked all inputs
    auth.position = serverPos;
    auth.velocity = Math::Vec3(0, 0, 0);
    auth.rotation = Math::Quat::Identity();

    auto replayFn = [&inputBuffer, dtSeconds](
        std::uint32_t seq, float dt, Client::Prediction::PredictedState& out) {
        ClientReplayInput(seq, dt, inputBuffer, out);
    };

    auto result = reconciliation.AcknowledgeAndReconcile(
        auth, dtSeconds, errorThresholdSq, replayFn);

    // After full reconciliation with all inputs acked, the client position
    // should exactly match the server position (no pending inputs to replay).
    EXPECT_NEAR(result.position.x, serverPos.x, 0.001f);
    EXPECT_NEAR(result.position.y, serverPos.y, 0.001f);
    EXPECT_NEAR(result.position.z, serverPos.z, 0.001f);

    // Expected values: 10 ticks * 1.0 * 0.05 = 0.5 (x), 10 * 0.5 * 0.05 = 0.25 (z)
    EXPECT_NEAR(serverPos.x, 0.5f, 0.001f);
    EXPECT_NEAR(serverPos.z, 0.25f, 0.001f);
}

} // namespace SagaEngine::Testing
