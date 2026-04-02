#include "SagaServer/Networking/Interest/InterestArea.h"
#include "SagaServer/Networking/Replication/ReplicationManager.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace SagaEngine::Networking::Interest;

class InterestTest : public ::testing::Test {
protected:
    std::unique_ptr<InterestManager> _interest;
    
    void SetUp() override {
        InterestConfig config;
        config.viewRadius = 100.0f;
        config.cullRadius = 150.0f;
        config.updateFrequency = 20.0f;
        _interest = std::make_unique<InterestManager>(config);
    }
};

TEST_F(InterestTest, CreateArea) {
    Vector3 center(0.0f, 0.0f, 0.0f);
    AreaId id = _interest->CreateArea(center, 50.0f);
    
    EXPECT_EQ(id, 1);
    EXPECT_EQ(_interest->GetEntitiesInArea(id).size(), 0);
}

TEST_F(InterestTest, AreaContains) {
    Vector3 center(0.0f, 0.0f, 0.0f);
    AreaId id = _interest->CreateArea(center, 50.0f);
    
    Vector3 inside(10.0f, 10.0f, 10.0f);
    Vector3 outside(100.0f, 100.0f, 100.0f);
    
    auto entities = _interest->GetEntitiesInArea(id);
    EXPECT_EQ(entities.size(), 0);
    
    _interest->UpdateEntityPosition(1, inside);
    _interest->Update(0.1f);
    
    EXPECT_EQ(_interest->GetEntityArea(1), id);
    
    _interest->UpdateEntityPosition(1, outside);
    _interest->Update(0.1f);
    
    EXPECT_NE(_interest->GetEntityArea(1), id);
}

TEST_F(InterestTest, ClientSubscription) {
    Vector3 center(0.0f, 0.0f, 0.0f);
    AreaId areaId = _interest->CreateArea(center, 50.0f);
    
    ClientId client = 1;
    _interest->SubscribeClientToArea(client, areaId);
    
    auto areas = _interest->GetClientSubscribedAreas(client);
    EXPECT_EQ(areas.size(), 1);
    EXPECT_EQ(areas[0], areaId);
    
    _interest->UnsubscribeClientFromArea(client, areaId);
    areas = _interest->GetClientSubscribedAreas(client);
    EXPECT_EQ(areas.size(), 0);
}

TEST_F(InterestTest, VisibleEntities) {
    Vector3 center(0.0f, 0.0f, 0.0f);
    AreaId areaId = _interest->CreateArea(center, 100.0f);
    
    ClientId client = 1;
    _interest->SubscribeClientToArea(client, areaId);
    
    EntityId entity1 = 1;
    EntityId entity2 = 2;
    EntityId entity3 = 3;
    
    _interest->UpdateEntityPosition(client, Vector3(0.0f, 0.0f, 0.0f));
    _interest->UpdateEntityPosition(entity1, Vector3(10.0f, 0.0f, 0.0f));   // Close
    _interest->UpdateEntityPosition(entity2, Vector3(50.0f, 0.0f, 0.0f));   // Medium
    _interest->UpdateEntityPosition(entity3, Vector3(200.0f, 0.0f, 0.0f));  // Far (culled)
    
    _interest->Update(0.1f);
    
    auto visible = _interest->GetVisibleEntities(client);
    
    EXPECT_EQ(visible.size(), 2);
    EXPECT_TRUE(std::find(visible.begin(), visible.end(), entity1) != visible.end());
    EXPECT_TRUE(std::find(visible.begin(), visible.end(), entity2) != visible.end());
    EXPECT_TRUE(std::find(visible.begin(), visible.end(), entity3) == visible.end());
}

TEST_F(InterestTest, MultipleAreas) {
    AreaId area1 = _interest->CreateArea(Vector3(0.0f, 0.0f, 0.0f), 50.0f);
    AreaId area2 = _interest->CreateArea(Vector3(100.0f, 0.0f, 0.0f), 50.0f);
    
    ClientId client = 1;
    _interest->SubscribeClientToArea(client, area1);
    _interest->SubscribeClientToArea(client, area2);
    
    _interest->UpdateEntityPosition(1, Vector3(0.0f, 0.0f, 0.0f));
    _interest->UpdateEntityPosition(2, Vector3(100.0f, 0.0f, 0.0f));
    
    _interest->Update(0.1f);
    
    auto visible = _interest->GetVisibleEntities(client);
    EXPECT_EQ(visible.size(), 2);
}

TEST_F(InterestTest, EntityMovement) {
    AreaId area1 = _interest->CreateArea(Vector3(0.0f, 0.0f, 0.0f), 50.0f);
    AreaId area2 = _interest->CreateArea(Vector3(100.0f, 0.0f, 0.0f), 50.0f);
    
    EntityId entity = 1;
    _interest->UpdateEntityPosition(entity, Vector3(0.0f, 0.0f, 0.0f));
    _interest->Update(0.1f);
    
    EXPECT_EQ(_interest->GetEntityArea(entity), area1);
    
    _interest->UpdateEntityPosition(entity, Vector3(100.0f, 0.0f, 0.0f));
    _interest->Update(0.1f);
    
    EXPECT_EQ(_interest->GetEntityArea(entity), area2);
}

TEST_F(InterestTest, DestroyArea) {
    AreaId areaId = _interest->CreateArea(Vector3(0.0f, 0.0f, 0.0f), 50.0f);
    ClientId client = 1;
    _interest->SubscribeClientToArea(client, areaId);
    
    _interest->DestroyArea(areaId);
    
    auto areas = _interest->GetClientSubscribedAreas(client);
    EXPECT_EQ(areas.size(), 0);
    EXPECT_EQ(_interest->GetEntitiesInArea(areaId).size(), 0);
}

TEST_F(InterestTest, ReplicationIntegration) {
    auto replication = std::make_unique<SagaEngine::Networking::Replication::ReplicationManager>();
    replication->Initialize(100);
    replication->SetInterestManager(_interest.get());
    
    AreaId areaId = _interest->CreateArea(Vector3(0.0f, 0.0f, 0.0f), 100.0f);
    ClientId client = 1;
    
    replication->AddClient(client);
    _interest->SubscribeClientToArea(client, areaId);
    
    EntityId entity1 = 1;
    EntityId entity2 = 2;
    
    _interest->UpdateEntityPosition(client, Vector3(0.0f, 0.0f, 0.0f));
    _interest->UpdateEntityPosition(entity1, Vector3(10.0f, 0.0f, 0.0f));
    _interest->UpdateEntityPosition(entity2, Vector3(200.0f, 0.0f, 0.0f));  // Far
    
    _interest->Update(0.1f);
    
    replication->RegisterEntityForReplication(entity1);
    replication->RegisterEntityForReplication(entity2);
    
    replication->MarkComponentDirty(entity1, 0);
    replication->MarkComponentDirty(entity2, 0);
    replication->GatherDirtyEntities(0.0f, true);
    
    int entitiesReceived = 0;
    replication->SendUpdates(client, [&](const uint8_t*, size_t size) {
        if (size > 0) entitiesReceived++;
    });
    
    EXPECT_EQ(entitiesReceived, 1);
    
    replication->Shutdown();
}