#include "SagaEngine/Networking/Core/Packet.h"
#include <gtest/gtest.h>
#include <cstring>

using namespace SagaEngine::Networking;

class PacketTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
    
    void TearDown() override {
    }
};

TEST_F(PacketTest, CreateAndValidate) {
    Packet packet(PacketType::HandshakeRequest);
    
    EXPECT_TRUE(packet.IsValid());
    EXPECT_EQ(packet.GetType(), PacketType::HandshakeRequest);
    EXPECT_EQ(packet.GetSequence(), 0);
    EXPECT_EQ(packet.GetPayloadSize(), 0);
}

TEST_F(PacketTest, WriteAndReadPrimitives) {
    Packet packet(PacketType::ComponentUpdate);
    
    // Write data
    uint32_t entityId = 12345;
    float health = 75.5f;
    uint8_t flags = 0x0F;
    
    packet.Write(entityId);
    packet.Write(health);
    packet.Write(flags);
    
    // Read data back
    size_t offset = 0;
    uint32_t readEntityId;
    float readHealth;
    uint8_t readFlags;
    
    EXPECT_TRUE(packet.Read(readEntityId, offset));
    EXPECT_TRUE(packet.Read(readHealth, offset));
    EXPECT_TRUE(packet.Read(readFlags, offset));
    
    EXPECT_EQ(readEntityId, entityId);
    EXPECT_FLOAT_EQ(readHealth, health);
    EXPECT_EQ(readFlags, flags);
}

TEST_F(PacketTest, SerializeAndDeserialize) {
    Packet original(PacketType::EntitySpawn);
    original.SetSequence(42);
    original.SetTimestamp(1234567890);
    
    uint32_t entityId = 999;
    original.Write(entityId);
    
    // Serialize
    auto data = original.GetSerializedData();
    
    // Deserialize
    Packet deserialized;
    bool success = Packet::Deserialize(data.data(), data.size(), deserialized);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(deserialized.IsValid());
    EXPECT_EQ(deserialized.GetType(), PacketType::EntitySpawn);
    EXPECT_EQ(deserialized.GetSequence(), 42);
    EXPECT_EQ(deserialized.GetTimestamp(), 1234567890);
    
    // Read payload
    size_t offset = 0;
    uint32_t readEntityId;
    EXPECT_TRUE(deserialized.Read(readEntityId, offset));
    EXPECT_EQ(readEntityId, entityId);
}

TEST_F(PacketTest, ChecksumValidation) {
    Packet packet(PacketType::InputCommand);
    packet.Write(static_cast<uint32_t>(1));
    packet.Write(static_cast<float>(2.0f));
    
    EXPECT_TRUE(packet.IsChecksumValid());
    
    // Corrupt payload
    auto data = packet.GetSerializedData();
    data[PacketHeader::HEADER_SIZE] ^= 0xFF;  // Flip bits in payload
    
    Packet corrupted;
    bool success = Packet::Deserialize(data.data(), data.size(), corrupted);
    
    EXPECT_FALSE(success);  // Should fail checksum validation
}

TEST_F(PacketTest, MaxPayloadSize) {
    Packet packet(PacketType::Custom);
    
    size_t maxPayload = Packet::GetMaxPayloadSize();
    std::vector<uint8_t> testData(maxPayload, 0xAB);
    
    bool success = packet.WriteBytes(testData.data(), testData.size());
    EXPECT_TRUE(success);
    
    // Try to exceed limit
    uint8_t extraByte = 0xFF;
    success = packet.WriteBytes(&extraByte, 1);
    EXPECT_FALSE(success);
}

TEST_F(PacketTest, SequenceNumbers) {
    Packet packet(PacketType::ComponentUpdate);
    
    for (uint32_t i = 0; i < 100; ++i) {
        packet.SetSequence(i);
        EXPECT_EQ(packet.GetSequence(), i);
    }
}

TEST_F(PacketTest, PacketTypeCoverage) {
    std::vector<PacketType> types = {
        PacketType::HandshakeRequest,
        PacketType::HandshakeResponse,
        PacketType::Disconnect,
        PacketType::KeepAlive,
        PacketType::Ack,
        PacketType::EntitySpawn,
        PacketType::ComponentUpdate,
        PacketType::Snapshot,
        PacketType::RPCRequest,
        PacketType::InputCommand,
        PacketType::AuthorityTransfer
    };
    
    for (auto type : types) {
        Packet packet(type);
        EXPECT_TRUE(packet.IsValid());
        EXPECT_EQ(packet.GetType(), type);
    }
}