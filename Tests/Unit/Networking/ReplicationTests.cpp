/// @file ReplicationTests.cpp
/// @brief Unit tests for replication manager behavior.

#include "SagaServer/Networking/Replication/ReplicationManager.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Simulation/Authority.h"
#include "SagaEngine/ECS/Component.h"

#include <gtest/gtest.h>
#include <cstring>
#include <mutex>

using namespace SagaEngine::Networking::Replication;

struct TestPositionComponent
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct TestHealthComponent
{
    float current{100.0f};
    float max{100.0f};
};

class ReplicationTest : public ::testing::Test
{
protected:
    std::unique_ptr<ReplicationManager> _replication;
    std::unique_ptr<SagaEngine::Simulation::WorldState> _world;
    std::unique_ptr<SagaEngine::Simulation::AuthorityManager> _authority;

    void SetUp() override
    {
        static std::once_flag once;
        std::call_once(once, []()
        {
            auto& componentRegistry = SagaEngine::ECS::ComponentRegistry::Instance();
            componentRegistry.Register<TestPositionComponent>(600001, "TestPositionComponent");
            componentRegistry.Register<TestHealthComponent>(600002, "TestHealthComponent");
        });

        _replication = std::make_unique<ReplicationManager>();
        _world = std::make_unique<SagaEngine::Simulation::WorldState>();

        _replication->Initialize(100);
        _replication->SetWorldState(_world.get());
        _replication->SetAuthorityManager(nullptr);
    }

    void TearDown() override
    {
        _replication->Shutdown();
    }
};

TEST_F(ReplicationTest, Initialize)
{
    EXPECT_EQ(_replication->GetReplicationFrequency(), 20.0f);
}

TEST_F(ReplicationTest, SetReplicationFrequency)
{
    _replication->SetReplicationFrequency(30.0f);
    EXPECT_EQ(_replication->GetReplicationFrequency(), 30.0f);
}

TEST_F(ReplicationTest, AddClient)
{
    _replication->AddClient(1);
    EXPECT_TRUE(_replication->HasClient(1));
}

TEST_F(ReplicationTest, RegisterEntity)
{
    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.id);
    EXPECT_TRUE(_world->IsAlive(entity.id));
}

TEST_F(ReplicationTest, MarkComponentDirty)
{
    _replication->AddClient(1);

    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.id);

    auto typeId = SagaEngine::ECS::ComponentRegistry::Instance().GetId<TestPositionComponent>();
    _replication->MarkComponentDirty(entity.id, typeId);

    _replication->GatherDirtyEntities(0.0f, true);

    bool sent = false;
    _replication->SendUpdates(1, [&](const uint8_t*, std::size_t)
    {
        sent = true;
    });

    EXPECT_TRUE(sent);
}

TEST_F(ReplicationTest, RPCRegistration)
{
    _replication->AddClient(1);

    bool rpcCalled = false;
    const uint32_t rpcId = _replication->RegisterRPC("TestRPC",
        [&](ClientId, EntityId, const uint8_t*, std::size_t)
        {
            rpcCalled = true;
        },
        false);

    _replication->ReceiveRPC(1, rpcId, 0, nullptr, 0);
    EXPECT_TRUE(rpcCalled);
}

TEST_F(ReplicationTest, AuthorityValidation)
{
    _replication->AddClient(1);

    bool rpcCalled = false;
    const uint32_t rpcId = _replication->RegisterRPC("AuthRPC",
        [&](ClientId, EntityId, const uint8_t*, std::size_t)
        {
            rpcCalled = true;
        },
        true);

    _replication->ReceiveRPC(1, rpcId, 0, nullptr, 0);
    EXPECT_TRUE(rpcCalled);
}

TEST_F(ReplicationTest, UnregisterEntity)
{
    _replication->AddClient(1);

    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.id);
    _replication->UnregisterEntity(entity.id);

    bool sent = false;
    _replication->SendUpdates(1, [&](const uint8_t*, std::size_t)
    {
        sent = true;
    });

    EXPECT_FALSE(sent);
}

TEST_F(ReplicationTest, MultipleClients)
{
    _replication->AddClient(1);
    _replication->AddClient(2);

    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.id);

    auto typeId = SagaEngine::ECS::ComponentRegistry::Instance().GetId<TestHealthComponent>();
    _replication->MarkComponentDirty(entity.id, typeId);

    _replication->GatherDirtyEntities(0.0f, true);

    int client1Sent = 0;
    int client2Sent = 0;

    _replication->SendUpdates(1, [&](const uint8_t*, std::size_t)
    {
        ++client1Sent;
    });
    _replication->SendUpdates(2, [&](const uint8_t*, std::size_t)
    {
        ++client2Sent;
    });

    EXPECT_EQ(client1Sent, 1);
    EXPECT_EQ(client2Sent, 1);
}

TEST_F(ReplicationTest, Shutdown)
{
    _replication->Shutdown();

    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.id);

    bool sent = false;
    _replication->SendUpdates(1, [&](const uint8_t*, std::size_t)
    {
        sent = true;
    });

    EXPECT_FALSE(sent);
}

TEST_F(ReplicationTest, ComponentSerialization)
{
    _replication->AddClient(1);

    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.id);

    TestPositionComponent pos{1.0f, 2.0f, 3.0f};
    _world->AddComponent<TestPositionComponent>(entity.id, pos);

    auto typeId = SagaEngine::ECS::ComponentRegistry::Instance().GetId<TestPositionComponent>();
    _replication->MarkComponentDirty(entity.id, typeId);

    _replication->GatherDirtyEntities(0.0f, true);

    bool sent = false;
    std::size_t sentSize = 0;
    _replication->SendUpdates(1, [&](const uint8_t*, std::size_t size)
    {
        sent = size > 0;
        sentSize = size;
    });

    EXPECT_TRUE(sent);
    EXPECT_GT(sentSize, 0u);
}

TEST_F(ReplicationTest, RemoveClient)
{
    _replication->AddClient(1);
    EXPECT_TRUE(_replication->HasClient(1));

    _replication->RemoveClient(1);
    EXPECT_FALSE(_replication->HasClient(1));
}