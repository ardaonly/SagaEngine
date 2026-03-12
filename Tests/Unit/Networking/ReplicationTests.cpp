#include "SagaEngine/Networking/Replication/ReplicationManager.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Simulation/Authority.h"
#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/Serialization.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>

using namespace SagaEngine::Networking::Replication;

struct TestPositionComponent {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct TestHealthComponent {
    float current{100.0f};
    float max{100.0f};
};

class ReplicationTest : public ::testing::Test {
protected:
    std::unique_ptr<ReplicationManager> _replication;
    std::unique_ptr<SagaEngine::Simulation::WorldState> _world;
    std::unique_ptr<SagaEngine::Simulation::AuthorityManager> _authority;

    void SetUp() override {
        _replication = std::make_unique<ReplicationManager>();
        _world = std::make_unique<SagaEngine::Simulation::WorldState>();
        _authority = std::make_unique<SagaEngine::Simulation::AuthorityManager>();
        
        _replication->Initialize(100);
        _replication->SetWorldState(_world.get());
        _replication->SetAuthorityManager(_authority.get());
        
        _authority->SetServerMode(true);
        _authority->SetClientId(1);
        
        SagaEngine::ECS::ComponentRegistry::Instance().RegisterComponent<TestPositionComponent>(
            "TestPosition",
            [](const TestPositionComponent& data, void* buffer, size_t bufferSize) -> size_t {
                if (bufferSize < sizeof(TestPositionComponent)) return 0;
                std::memcpy(buffer, &data, sizeof(TestPositionComponent));
                return sizeof(TestPositionComponent);
            },
            [](TestPositionComponent& data, const void* buffer, size_t bufferSize) -> size_t {
                if (bufferSize < sizeof(TestPositionComponent)) return 0;
                std::memcpy(&data, buffer, sizeof(TestPositionComponent));
                return sizeof(TestPositionComponent);
            },
            []() -> size_t { return sizeof(TestPositionComponent); }
        );
        
        SagaEngine::ECS::ComponentRegistry::Instance().RegisterComponent<TestHealthComponent>(
            "TestHealth",
            [](const TestHealthComponent& data, void* buffer, size_t bufferSize) -> size_t {
                if (bufferSize < sizeof(TestHealthComponent)) return 0;
                std::memcpy(buffer, &data, sizeof(TestHealthComponent));
                return sizeof(TestHealthComponent);
            },
            [](TestHealthComponent& data, const void* buffer, size_t bufferSize) -> size_t {
                if (bufferSize < sizeof(TestHealthComponent)) return 0;
                std::memcpy(&data, buffer, sizeof(TestHealthComponent));
                return sizeof(TestHealthComponent);
            },
            []() -> size_t { return sizeof(TestHealthComponent); }
        );
    }

    void TearDown() override {
        _replication->Shutdown();
    }
};

TEST_F(ReplicationTest, Initialize) {
    EXPECT_EQ(_replication->GetReplicationFrequency(), 20.0f);
}

TEST_F(ReplicationTest, SetReplicationFrequency) {
    _replication->SetReplicationFrequency(30.0f);
    EXPECT_EQ(_replication->GetReplicationFrequency(), 30.0f);
}

TEST_F(ReplicationTest, AddClient) {
    _replication->AddClient(1);
    EXPECT_TRUE(_replication->HasClient(1));
}

TEST_F(ReplicationTest, RegisterEntity) {
    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.GetId());
    EXPECT_TRUE(_world->IsEntityAlive(entity.GetId()));
}

TEST_F(ReplicationTest, MarkComponentDirty) {
    _replication->AddClient(1);
    
    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.GetId());
    
    auto typeId = SagaEngine::ECS::GetComponentTypeId<TestPositionComponent>();
    _replication->MarkComponentDirty(entity.GetId(), typeId);
    
    _replication->GatherDirtyEntities(0.0f, true);
    
    bool sent = false;
    _replication->SendUpdates(1, [&](const uint8_t*, size_t) {
        sent = true;
    });
    
    EXPECT_TRUE(sent);
}

TEST_F(ReplicationTest, RPCRegistration) {
    _replication->AddClient(1);
    
    bool rpcCalled = false;
    _replication->RegisterRPC("TestRPC", [&](ClientId, const uint8_t*, size_t) {
        rpcCalled = true;
    }, false);
    
    _replication->ReceiveRPC(1, 1, nullptr, 0);
    EXPECT_TRUE(rpcCalled);
}

TEST_F(ReplicationTest, AuthorityValidation) {
    _replication->AddClient(1);
    
    bool rpcCalled = false;
    _replication->RegisterRPC("AuthRPC", [&](ClientId, const uint8_t*, size_t) {
        rpcCalled = true;
    }, true);
    
    _replication->ReceiveRPC(1, 1, nullptr, 0);
    EXPECT_TRUE(rpcCalled);
}

TEST_F(ReplicationTest, UnregisterEntity) {
    _replication->AddClient(1);
    
    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.GetId());
    _replication->UnregisterEntity(entity.GetId());
    
    bool sent = false;
    _replication->SendUpdates(1, [&](const uint8_t*, size_t) {
        sent = true;
    });
    
    EXPECT_FALSE(sent);
}

TEST_F(ReplicationTest, MultipleClients) {
    _replication->AddClient(1);
    _replication->AddClient(2);
    
    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.GetId());
    
    auto typeId = SagaEngine::ECS::GetComponentTypeId<TestHealthComponent>();
    _replication->MarkComponentDirty(entity.GetId(), typeId);
    
    _replication->GatherDirtyEntities(0.0f, true);  // Force
    
    int client1Sent = 0, client2Sent = 0;
    _replication->SendUpdates(1, [&](const uint8_t*, size_t) { client1Sent++; });
    _replication->SendUpdates(2, [&](const uint8_t*, size_t) { client2Sent++; });
    
    EXPECT_EQ(client1Sent, 1);
    EXPECT_EQ(client2Sent, 1);
}

TEST_F(ReplicationTest, Shutdown) {
    _replication->Shutdown();
    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.GetId());
    
    bool sent = false;
    _replication->SendUpdates(1, [&](const uint8_t*, size_t) {
        sent = true;
    });
    
    EXPECT_FALSE(sent);
}

TEST_F(ReplicationTest, ComponentSerialization) {
    _replication->AddClient(1);
    
    auto entity = _world->CreateEntity();
    _replication->RegisterEntityForReplication(entity.GetId());
    
    TestPositionComponent pos{1.0f, 2.0f, 3.0f};
    _world->AddComponent(entity.GetId(), pos);
    
    auto typeId = SagaEngine::ECS::GetComponentTypeId<TestPositionComponent>();
    _replication->MarkComponentDirty(entity.GetId(), typeId);
    
    _replication->GatherDirtyEntities(0.0f, true);
    
    bool sent = false;
    size_t sentSize = 0;
    _replication->SendUpdates(1, [&](const uint8_t* data, size_t size) {
        sent = size > 0;
        sentSize = size;
    });
    
    EXPECT_TRUE(sent);
    EXPECT_GT(sentSize, 0);
}

TEST_F(ReplicationTest, RemoveClient) {
    _replication->AddClient(1);
    EXPECT_TRUE(_replication->HasClient(1));
    
    _replication->RemoveClient(1);
    EXPECT_FALSE(_replication->HasClient(1));
}