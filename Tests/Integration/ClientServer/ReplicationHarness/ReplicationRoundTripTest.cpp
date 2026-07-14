/// @file ReplicationRoundTripTest.cpp
/// @brief Round-trip tests for the replication pipeline: encode -> decode -> verify.

#include "ReplicationRoundTripTest.h"

#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"
#include "SagaEngine/Client/Replication/WorldSnapshotWire.h"

#include <cstring>
#include <random>
#include <vector>

namespace SagaEngine::Client::Replication {

namespace {

struct TestRng
{
    std::mt19937_64 engine;
    explicit TestRng(std::uint64_t seed) : engine(seed) {}

    std::uint32_t Uint32(std::uint32_t min, std::uint32_t max) {
        std::uniform_int_distribution<std::uint32_t> dist(min, max);
        return dist(engine);
    }

    std::vector<std::uint8_t> Bytes(std::size_t len) {
        std::vector<std::uint8_t> out(len);
        for (auto& b : out)
            b = static_cast<std::uint8_t>(engine() & 0xFF);
        return out;
    }
};

/// Build a synthetic delta payload for testing.
std::vector<std::uint8_t> BuildTestDeltaPayload(
    std::uint32_t entityCount,
    TestRng& rng)
{
    std::vector<std::uint8_t> payload;

    // Build entity data section first.
    std::vector<std::uint8_t> entityData;
    for (std::uint32_t e = 0; e < entityCount; ++e)
    {
        std::uint32_t entityId = 1000 + e;
        std::uint16_t compCount = 2;

        // Entity header
        entityData.push_back(static_cast<std::uint8_t>(entityId));
        entityData.push_back(static_cast<std::uint8_t>(entityId >> 8));
        entityData.push_back(static_cast<std::uint8_t>(entityId >> 16));
        entityData.push_back(static_cast<std::uint8_t>(entityId >> 24));
        entityData.push_back(static_cast<std::uint8_t>(compCount));
        entityData.push_back(static_cast<std::uint8_t>(compCount >> 8));

        // Component 1: Position (typeId=1, 12 bytes)
        entityData.push_back(1); entityData.push_back(0);  // typeId
        entityData.push_back(12); entityData.push_back(0); // dataLen
        auto posData = rng.Bytes(12);
        entityData.insert(entityData.end(), posData.begin(), posData.end());

        // Component 2: Velocity (typeId=2, 12 bytes)
        entityData.push_back(2); entityData.push_back(0);  // typeId
        entityData.push_back(12); entityData.push_back(0); // dataLen
        auto velData = rng.Bytes(12);
        entityData.insert(entityData.end(), velData.begin(), velData.end());
    }

    // Prepend 40-byte delta header.
    // version(4) + serverTick(8) + sequence(8) + ackSequence(8) + entityCount(4) + payloadBytes(4) + schemaVersion(4)
    std::uint32_t version = 1;
    std::uint64_t serverTick = 1000;
    std::uint64_t sequence = 500;
    std::uint64_t ackSequence = 499;
    std::uint32_t payloadBytes = static_cast<std::uint32_t>(entityData.size());
    std::uint32_t schemaVersion = 1;

    auto WriteLE32 = [&](std::uint32_t val) {
        payload.push_back(static_cast<std::uint8_t>(val));
        payload.push_back(static_cast<std::uint8_t>(val >> 8));
        payload.push_back(static_cast<std::uint8_t>(val >> 16));
        payload.push_back(static_cast<std::uint8_t>(val >> 24));
    };
    auto WriteLE64 = [&](std::uint64_t val) {
        for (int i = 0; i < 8; ++i)
            payload.push_back(static_cast<std::uint8_t>(val >> (i * 8)));
    };

    WriteLE32(version);
    WriteLE64(serverTick);
    WriteLE64(sequence);
    WriteLE64(ackSequence);
    WriteLE32(entityCount);
    WriteLE32(payloadBytes);
    WriteLE32(schemaVersion);

    // Append entity data.
    payload.insert(payload.end(), entityData.begin(), entityData.end());

    return payload;
}

/// Build a synthetic full snapshot payload for testing.
std::vector<std::uint8_t> BuildTestSnapshotPayload(
    std::uint32_t entityCount,
    std::uint32_t& outCRC,
    TestRng& rng)
{
    std::vector<std::uint8_t> payload;

    // Extended header (88 bytes)
    payload.resize(88, 0);

    // Magic
    payload[0] = 0x57; payload[1] = 0x53; payload[2] = 0x47; payload[3] = 0x53;
    // Version
    payload[4] = 3; payload[5] = 0; payload[6] = 0; payload[7] = 0;
    // Server tick
    std::uint64_t tick = 1000;
    std::memcpy(payload.data() + 8, &tick, 8);
    // Entity count
    std::memcpy(payload.data() + 32, &entityCount, 4);
    // Byte length (will update after building entity data)
    // Schema version
    std::uint32_t schemaVer = 1;
    std::memcpy(payload.data() + 48, &schemaVer, 4);
    // Protocol version
    std::uint32_t protoVer = 1;
    std::memcpy(payload.data() + 52, &protoVer, 4);

    std::size_t dataStart = 88;

    // Entity data
    for (std::uint32_t e = 0; e < entityCount; ++e)
    {
        std::uint32_t entityId = 1000 + e;
        std::uint16_t compCount = 2;

        payload.push_back(static_cast<std::uint8_t>(entityId));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 8));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 16));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 24));
        payload.push_back(static_cast<std::uint8_t>(compCount));
        payload.push_back(static_cast<std::uint8_t>(compCount >> 8));

        // Component 1
        payload.push_back(1); payload.push_back(0);
        payload.push_back(12); payload.push_back(0);
        auto d1 = rng.Bytes(12);
        payload.insert(payload.end(), d1.begin(), d1.end());

        // Component 2
        payload.push_back(2); payload.push_back(0);
        payload.push_back(12); payload.push_back(0);
        auto d2 = rng.Bytes(12);
        payload.insert(payload.end(), d2.begin(), d2.end());
    }

    std::uint64_t byteLength = payload.size() - dataStart;
    std::memcpy(payload.data() + 40, &byteLength, 8);

    // Compute CRC32 of entity data
    outCRC = ComputeCRC32(payload.data() + dataStart, byteLength);
    std::memcpy(payload.data() + 36, &outCRC, 4);

    return payload;
}

} // namespace

// ─── Delta round-trip test ──────────────────────────────────────────────────

RoundTripResult TestDeltaRoundTrip(std::uint32_t entityCount, std::uint64_t seed) noexcept
{
    RoundTripResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    TestRng rng(seed);
    auto payload = BuildTestDeltaPayload(entityCount, rng);
    result.bytesEncoded = payload.size();

    DeltaDecodeResult decoded = DecodeDeltaWire(payload.data(), payload.size(), 1024);

    if (!decoded.success)
    {
        result.failureDetail = "Decode failed: " + decoded.error;
        return result;
    }

    result.bytesDecoded = decoded.decoded.payload.size();

    // Verify entity count.
    if (decoded.decoded.entityCount != entityCount)
    {
        result.failureDetail = "Entity count mismatch: expected " +
            std::to_string(entityCount) + ", got " +
            std::to_string(decoded.decoded.entityCount);
        return result;
    }

    // Verify each entity has the expected components.
    for (const auto& entity : decoded.decoded.entities)
    {
        if (entity.components.size() != 2)
        {
            result.failureDetail = "Entity " + std::to_string(entity.entityId) +
                " has " + std::to_string(entity.components.size()) + " components, expected 2";
            return result;
        }
    }

    result.success = true;
    return result;
}

// ─── Snapshot round-trip test ───────────────────────────────────────────────

RoundTripResult TestSnapshotRoundTrip(std::uint32_t entityCount, std::uint64_t seed) noexcept
{
    RoundTripResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    TestRng rng(seed);
    std::uint32_t crc = 0;
    auto payload = BuildTestSnapshotPayload(entityCount, crc, rng);
    result.bytesEncoded = payload.size();

    SnapshotDecodeResult decoded = DecodeSnapshotWire(payload.data(), payload.size(), 1024);

    if (!decoded.success)
    {
        result.failureDetail = "Decode failed: " + decoded.error;
        return result;
    }

    result.bytesDecoded = decoded.decoded.payload.size();

    // Verify entity count.
    if (decoded.decoded.entityCount != entityCount)
    {
        result.failureDetail = "Entity count mismatch";
        return result;
    }

    result.success = true;
    return result;
}

// ─── CRC integrity test ─────────────────────────────────────────────────────

RoundTripResult TestCrcIntegrity(std::uint64_t seed) noexcept
{
    RoundTripResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    TestRng rng(seed);
    std::uint32_t crc = 0;
    auto payload = BuildTestSnapshotPayload(10, crc, rng);

    // Corrupt a byte in the payload.
    payload[100] ^= 0xFF;

    SnapshotDecodeResult decoded = DecodeSnapshotWire(payload.data(), payload.size(), 1024);

    if (decoded.success)
    {
        result.failureDetail = "Decoded corrupted payload -- CRC check failed";
        return result;
    }

    result.success = true;  // Expected failure = test passes.
    return result;
}

// ─── Schema forward compatibility test ──────────────────────────────────────

RoundTripResult TestSchemaForwardCompat(std::uint64_t seed) noexcept
{
    RoundTripResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    TestRng rng(seed);

    // Build a payload with an unknown component type (typeId=999).
    // Must include proper 40-byte delta header.
    std::vector<std::uint8_t> payload;

    // Delta header (40 bytes): version(4) + serverTick(8) + sequence(8) + ackSequence(8) + entityCount(4) + payloadBytes(4) + schemaVersion(4)
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0); // version=1
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0); // serverTick=1
    payload.push_back(0); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0); // sequence=1
    payload.push_back(0); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    for (int i = 0; i < 8; ++i) payload.push_back(0); // ackSequence=0
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0); // entityCount=1
    std::size_t payloadSizeOffset = payload.size();
    payload.push_back(0); payload.push_back(0); payload.push_back(0); payload.push_back(0); // payloadBytes (TBD)
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0); // schemaVersion=1

    // Entity data section (18 bytes)
    std::size_t entityStart = payload.size();
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0); // entityId=1
    payload.push_back(1); payload.push_back(0); // componentCount=1
    payload.push_back(0xE7); payload.push_back(0x03); // typeId=999
    payload.push_back(8); payload.push_back(0); // dataLen=8
    for (int i = 0; i < 8; ++i)
        payload.push_back(static_cast<std::uint8_t>(rng.Uint32(0, 255)));

    // Update payloadBytes in header.
    std::size_t entitySize = payload.size() - entityStart;
    payload[payloadSizeOffset + 0] = static_cast<std::uint8_t>(entitySize & 0xFF);
    payload[payloadSizeOffset + 1] = static_cast<std::uint8_t>((entitySize >> 8) & 0xFF);
    payload[payloadSizeOffset + 2] = static_cast<std::uint8_t>((entitySize >> 16) & 0xFF);
    payload[payloadSizeOffset + 3] = static_cast<std::uint8_t>((entitySize >> 24) & 0xFF);

    // Decode with maxTypeId=100 (should skip typeId=999).
    DeltaDecodeResult decoded = DecodeDeltaWire(payload.data(), payload.size(), 100);

    if (!decoded.success)
    {
        result.failureDetail = "Forward-compatible decode failed: " + decoded.error;
        return result;
    }

    // The entity should be present but with zero components (typeId=999 skipped).
    if (decoded.decoded.entities.size() != 1)
    {
        result.failureDetail = "Expected 1 entity, got " +
            std::to_string(decoded.decoded.entities.size());
        return result;
    }

    if (!decoded.decoded.entities[0].components.empty())
    {
        result.failureDetail = "Unknown component type was not skipped";
        return result;
    }

    result.success = true;
    return result;
}

// ─── Run all tests ──────────────────────────────────────────────────────────

std::vector<RoundTripResult> RunAllRoundTripTests(std::uint64_t seed) noexcept
{
    std::vector<RoundTripResult> results;
    results.reserve(4);

    results.push_back(TestDeltaRoundTrip(100, seed));
    results.push_back(TestSnapshotRoundTrip(100, seed));
    results.push_back(TestCrcIntegrity(seed));
    results.push_back(TestSchemaForwardCompat(seed));

    return results;
}

} // namespace SagaEngine::Client::Replication
