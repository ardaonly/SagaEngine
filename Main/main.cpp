// main.cpp — temporary debug/testing area

#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Networking/Core/Packet.h"
#include "SagaEngine/Networking/Core/NetworkTransport.h"
#include "SagaEngine/Core/Time/Time.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

using namespace SagaEngine;
using namespace SagaEngine::Networking;

// ============================================================================
// Test Results Tracker
// ============================================================================
struct TestResults {
    uint32_t passed = 0;
    uint32_t failed = 0;
    
    void Pass(const char* test) {
        passed++;
        std::printf("[PASS] %s\n", test);
    }
    
    void Fail(const char* test, const char* reason) {
        failed++;
        std::printf("[FAIL] %s - %s\n", test, reason);
    }
    
    void Summary() {
        std::printf("\n========== TEST SUMMARY ==========\n");
        std::printf("Passed: %u\n", passed);
        std::printf("Failed: %u\n", failed);
        std::printf("Total:  %u\n", passed + failed);
        std::printf("==================================\n");
    }
};

static TestResults g_Results;

// ============================================================================
// Packet Tests
// ============================================================================
void TestPacketCreation() {
    Packet pkt(PacketType::HandshakeRequest);
    
    if (pkt.IsValid() && pkt.GetType() == PacketType::HandshakeRequest) {
        g_Results.Pass("PacketCreation");
    } else {
        g_Results.Fail("PacketCreation", "Invalid packet after creation");
    }
}

void TestPacketSerialization() {
    Packet original(PacketType::ComponentUpdate);
    original.SetSequence(42);
    original.SetTimestamp(1234567890);
    
    uint32_t entityId = 999;
    float health = 75.5f;
    original.Write(entityId);
    original.Write(health);
    
    auto data = original.GetSerializedData();
    
    Packet deserialized;
    bool success = Packet::Deserialize(data.data(), data.size(), deserialized);
    
    if (!success) {
        g_Results.Fail("PacketSerialization", "Deserialize failed");
        return;
    }
    
    size_t offset = 0;
    uint32_t readEntityId;
    float readHealth;
    
    deserialized.Read(readEntityId, offset);
    deserialized.Read(readHealth, offset);
    
    if (readEntityId == entityId && readHealth == health) {
        g_Results.Pass("PacketSerialization");
    } else {
        g_Results.Fail("PacketSerialization", "Data mismatch after deserialize");
    }
}

void TestPacketChecksum() {
    Packet pkt(PacketType::InputCommand);
    pkt.Write(static_cast<uint32_t>(1));
    pkt.Write(static_cast<float>(2.0f));
    
    if (!pkt.IsChecksumValid()) {
        g_Results.Fail("PacketChecksum", "Initial checksum invalid");
        return;
    }
    
    auto data = pkt.GetSerializedData();
    data[PACKET_HEADER_SIZE] ^= 0xFF;
    
    Packet corrupted;
    bool success = Packet::Deserialize(data.data(), data.size(), corrupted);
    
    if (!success) {
        g_Results.Pass("PacketChecksum");
    } else {
        g_Results.Fail("PacketChecksum", "Corrupted packet passed validation");
    }
}

void TestPacketMaxSize() {
    Packet pkt(PacketType::Snapshot);
    
    size_t maxPayload = Packet::GetMaxPayloadSize();
    std::vector<uint8_t> testData(maxPayload, 0xAB);
    
    bool success = pkt.WriteBytes(testData.data(), testData.size());
    
    if (success) {
        g_Results.Pass("PacketMaxSize");
    } else {
        g_Results.Fail("PacketMaxSize", "Failed to write max payload");
    }
}

// ============================================================================
// Transport Tests
// ============================================================================
void TestTransportCreate() {
    auto transport = TransportFactory::Create(true);
    
    if (transport != nullptr) {
        g_Results.Pass("TransportCreate");
    } else {
        g_Results.Fail("TransportCreate", "Factory returned null");
    }
}

void TestTransportInitialize() {
    auto transport = TransportFactory::Create(true);
    
    NetworkConfig config;
    config.keepAliveIntervalMs = 5000;
    config.maxPacketSize = 1400;
    
    bool success = transport->Initialize(config);
    
    if (success) {
        g_Results.Pass("TransportInitialize");
        transport->Shutdown();
    } else {
        g_Results.Fail("TransportInitialize", "Initialize failed");
    }
}

void TestTransportConnect() {
    std::atomic<bool> serverReady{false};
    std::atomic<bool> clientConnected{false};
    std::atomic<bool> serverReceived{false};
    
    NetworkConfig config;
    config.keepAliveIntervalMs = 1000;
    config.connectTimeoutMs = 3000;
    
    auto server = TransportFactory::Create(true);
    server->Initialize(config);
    
    server->SetOnPacketReceived([&](ConnectionId, const uint8_t*, size_t) {
        serverReceived.store(true);
    });
    
    std::thread serverThread([&]() {
        serverReady.store(true);
        
        while (!serverReceived.load() && server->GetState() != ConnectionState::Failed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    while (!serverReady.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto client = TransportFactory::Create(true);
    client->Initialize(config);
    
    client->SetOnConnected([&](ConnectionId) {
        clientConnected.store(true);
        
        Packet testPacket(PacketType::KeepAlive);
        testPacket.Write(static_cast<uint32_t>(12345));
        client->Send(testPacket.GetData(), testPacket.GetTotalSize());
    });
    
    bool connectSuccess = client->Connect(NetworkAddress("127.0.0.1", 0));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (connectSuccess && clientConnected.load()) {
        g_Results.Pass("TransportConnect");
    } else {
        g_Results.Fail("TransportConnect", "Connection failed");
    }
    
    client->Shutdown();
    server->Shutdown();
    serverThread.join();
}

void TestTransportSendReceive() {
    std::atomic<bool> dataReceived{false};
    std::atomic<uint32_t> receivedValue{0};
    
    NetworkConfig config;
    config.maxPacketSize = 1400;
    
    auto receiver = TransportFactory::Create(true);
    receiver->Initialize(config);
    
    receiver->SetOnPacketReceived([&](ConnectionId, const uint8_t* data, size_t size) {
        Packet pkt;
        if (Packet::Deserialize(data, size, pkt)) {
            uint32_t value;
            size_t offset = 0;
            if (pkt.Read(value, offset)) {
                receivedValue.store(value);
                dataReceived.store(true);
            }
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto sender = TransportFactory::Create(true);
    sender->Initialize(config);
    
    Packet testPacket(PacketType::Custom);
    uint32_t testValue = 54321;
    testPacket.Write(testValue);
    
    sender->Send(testPacket.GetData(), testPacket.GetTotalSize());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    if (dataReceived.load() && receivedValue.load() == testValue) {
        g_Results.Pass("TransportSendReceive");
    } else {
        g_Results.Fail("TransportSendReceive", "Data not received correctly");
    }
    
    receiver->Shutdown();
    sender->Shutdown();
}

void TestTransportStatistics() {
    auto transport = TransportFactory::Create(true);
    
    NetworkConfig config;
    transport->Initialize(config);
    
    auto stats = transport->GetStatistics();
    
    if (stats.packetsSent == 0 && stats.packetsReceived == 0) {
        g_Results.Pass("TransportStatistics");
    } else {
        g_Results.Fail("TransportStatistics", "Initial stats not zero");
    }
    
    transport->Shutdown();
}

// ============================================================================
// Integration Tests
// ============================================================================
void TestEndToEndCommunication() {
    std::atomic<uint32_t> messagesSent{0};
    std::atomic<uint32_t> messagesReceived{0};
    
    NetworkConfig config;
    config.maxPacketSize = 1400;
    config.keepAliveIntervalMs = 0;
    
    auto server = TransportFactory::Create(true);
    server->Initialize(config);
    
    server->SetOnPacketReceived([&](ConnectionId, const uint8_t* data, size_t size) {
        messagesReceived.fetch_add(1);
    });
    
    auto client = TransportFactory::Create(true);
    client->Initialize(config);
    
    client->SetOnConnected([&](ConnectionId) {
        for (int i = 0; i < 10; ++i) {
            Packet pkt(PacketType::InputCommand);
            pkt.Write(static_cast<uint32_t>(i));
            client->Send(pkt.GetData(), pkt.GetTotalSize());
            messagesSent.fetch_add(1);
        }
    });
    
    client->Connect(NetworkAddress("127.0.0.1", 0));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (messagesSent.load() == 10 && messagesReceived.load() == 10) {
        g_Results.Pass("EndToEndCommunication");
    } else {
        g_Results.Fail("EndToEndCommunication", 
            std::string("Sent: " + std::to_string(messagesSent.load()) + 
                       ", Received: " + std::to_string(messagesReceived.load())).c_str());
    }
    
    client->Shutdown();
    server->Shutdown();
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main(int argc, char* argv[]) {
    std::printf("\n");
    std::printf("========================================\n");
    std::printf("   SAGAENGINE NETWORK TEST SUITE\n");
    std::printf("========================================\n");
    std::printf("Build: %s %s\n", __DATE__, __TIME__);
    std::printf("========================================\n\n");
    
    Core::Log::Init("network_tests.log");
    Core::Log::SetLevel(Core::Log::Level::Info);
    
    std::printf("=== PACKET TESTS ===\n");
    TestPacketCreation();
    TestPacketSerialization();
    TestPacketChecksum();
    TestPacketMaxSize();
    
    std::printf("\n=== TRANSPORT TESTS ===\n");
    TestTransportCreate();
    TestTransportInitialize();
    TestTransportConnect();
    TestTransportSendReceive();
    TestTransportStatistics();
    
    std::printf("\n=== INTEGRATION TESTS ===\n");
    TestEndToEndCommunication();
    
    std::printf("\n");
    g_Results.Summary();
    
    Core::Log::Shutdown();
    
    if (g_Results.failed > 0) {
        return 1;
    }
    
    std::printf("\n[SUCCESS] All tests passed!\n");
    return 0;
}