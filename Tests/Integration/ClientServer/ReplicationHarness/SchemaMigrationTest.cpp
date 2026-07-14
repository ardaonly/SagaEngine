/// @file SchemaMigrationTest.cpp
/// @brief Schema migration and backward compatibility tests.

#include "SchemaMigrationTest.h"

#include "SagaEngine/Client/Replication/SchemaMigrationPolicy.h"
#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"

#include <vector>

namespace SagaEngine::Client::Replication {

// ─── Major mismatch test ────────────────────────────────────────────────────

MigrationTestResult TestMajorMismatch() noexcept
{
    MigrationTestResult result;

    SchemaMigrationPolicy policy;
    policy.SetClientVersion({1, 0, 0}, {1, 5, 0});  // Client supports major 1.

    SchemaVersion negotiated;
    CompatibilityResult compat = policy.CheckCompatibility({2, 0, 0}, &negotiated);

    if (compat != CompatibilityResult::Incompatible)
    {
        result.failureDetail = "Major version mismatch was not detected as incompatible";
        return result;
    }

    result.success = true;
    return result;
}

// ─── Minor forward compatibility test ───────────────────────────────────────

MigrationTestResult TestMinorForwardCompat() noexcept
{
    MigrationTestResult result;

    SchemaMigrationPolicy policy;
    policy.SetClientVersion({1, 0, 0}, {1, 3, 0});  // Client supports up to minor 3.

    SchemaVersion negotiated;
    CompatibilityResult compat = policy.CheckCompatibility({1, 5, 0}, &negotiated);

    if (compat != CompatibilityResult::ForwardCompatible)
    {
        result.failureDetail = "Server minor > client minor was not detected as forward-compatible";
        return result;
    }

    // Negotiated version should use client's minor.
    if (negotiated.minor != 3)
    {
        result.failureDetail = "Negotiated minor should be 3 (client's max), got " +
            std::to_string(negotiated.minor);
        return result;
    }

    result.success = true;
    return result;
}

// ─── Minor backward compatibility test ──────────────────────────────────────

MigrationTestResult TestMinorBackwardCompat() noexcept
{
    MigrationTestResult result;

    SchemaMigrationPolicy policy;
    policy.SetClientVersion({1, 0, 0}, {1, 5, 0});  // Client supports up to minor 5.

    SchemaVersion negotiated;
    CompatibilityResult compat = policy.CheckCompatibility({1, 3, 0}, &negotiated);

    if (compat != CompatibilityResult::BackwardCompatible)
    {
        result.failureDetail = "Client minor > server minor was not detected as backward-compatible";
        return result;
    }

    // Negotiated version should use server's minor.
    if (negotiated.minor != 3)
    {
        result.failureDetail = "Negotiated minor should be 3 (server's), got " +
            std::to_string(negotiated.minor);
        return result;
    }

    result.success = true;
    return result;
}

// ─── Patch negotiation test ─────────────────────────────────────────────────

MigrationTestResult TestPatchNegotiation() noexcept
{
    MigrationTestResult result;

    SchemaMigrationPolicy policy;
    policy.SetClientVersion({1, 0, 0}, {1, 3, 5});

    SchemaVersion negotiated;
    CompatibilityResult compat = policy.CheckCompatibility({1, 3, 8}, &negotiated);

    if (compat != CompatibilityResult::Compatible)
    {
        result.failureDetail = "Same minor versions should be fully compatible";
        return result;
    }

    // Patch should use server's patch level.
    if (negotiated.patch != 8)
    {
        result.failureDetail = "Negotiated patch should be 8 (server's), got " +
            std::to_string(negotiated.patch);
        return result;
    }

    result.success = true;
    return result;
}

// ─── Component skip test ────────────────────────────────────────────────────

MigrationTestResult TestComponentSkip() noexcept
{
    MigrationTestResult result;

    // Build a payload with an unknown component type (typeId=999).
    // Must include proper delta header + entity data.
    std::vector<std::uint8_t> payload;

    // Delta header fields (40 bytes):
    // version(4) + serverTick(8) + sequence(8) + ackSequence(8) + entityCount(4) + payloadBytes(4) + schemaVersion(4)
    // version = kDeltaWireVersion = 1
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    // serverTick = 1
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    payload.push_back(0); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    // sequence = 1
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    payload.push_back(0); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    // ackSequence = 0
    for (int i = 0; i < 8; ++i)
        payload.push_back(0);
    // entityCount = 1
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    // payloadBytes = will be calculated below
    std::size_t payloadSizeOffset = payload.size();
    payload.push_back(0); payload.push_back(0); payload.push_back(0); payload.push_back(0);
    // schemaVersion = 1
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0);

    // Entity section: entityId(4) + componentCount(2) + component data
    std::size_t entityStart = payload.size();
    payload.push_back(1); payload.push_back(0); payload.push_back(0); payload.push_back(0);  // entityId=1
    payload.push_back(1); payload.push_back(0);  // componentCount=1

    // Component: typeId=999, dataLen=8
    payload.push_back(0xE7); payload.push_back(0x03);  // typeId=999 (little-endian)
    payload.push_back(8); payload.push_back(0);        // dataLen=8
    for (int i = 0; i < 8; ++i)
        payload.push_back(static_cast<std::uint8_t>(i));

    // Calculate actual payload size (entity section size)
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

    // Entity should be present but with zero components.
    if (decoded.decoded.entities.size() != 1)
    {
        result.failureDetail = "Expected 1 entity, got " +
            std::to_string(decoded.decoded.entities.size());
        return result;
    }

    if (!decoded.decoded.entities[0].components.empty())
    {
        result.failureDetail = "Unknown component typeId=999 was not skipped";
        return result;
    }

    result.success = true;
    return result;
}

// ─── Run all tests ──────────────────────────────────────────────────────────

std::vector<MigrationTestResult> RunAllMigrationTests() noexcept
{
    std::vector<MigrationTestResult> results;
    results.reserve(5);

    results.push_back(TestMajorMismatch());
    results.push_back(TestMinorForwardCompat());
    results.push_back(TestMinorBackwardCompat());
    results.push_back(TestPatchNegotiation());
    results.push_back(TestComponentSkip());

    return results;
}

} // namespace SagaEngine::Client::Replication
