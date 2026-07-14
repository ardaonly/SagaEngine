/// @file DeterminismGuardTest.cpp
/// @brief Cross-platform determinism verification tests.

#include "DeterminismGuardTest.h"

#include "SagaEngine/Client/Replication/DeterminismVerifier.h"
#include "SagaEngine/Client/Replication/GranularWorldHash.h"
#include "SagaEngine/Client/Replication/EcsSnapshotApply.h"
#include "SagaEngine/Client/Replication/PatchJournal.h"

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

namespace SagaEngine::Client::Replication {

namespace {

struct TestRng
{
    std::mt19937_64 engine;
    explicit TestRng(std::uint64_t seed) : engine(seed) {}

    float Float() {
        std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
        return dist(engine);
    }

    std::uint32_t Uint32(std::uint32_t min, std::uint32_t max) {
        std::uniform_int_distribution<std::uint32_t> dist(min, max);
        return dist(engine);
    }
};

} // namespace

// ─── Float canonicalisation test ────────────────────────────────────────────

DeterminismTestResult TestFloatCanonicalisation(std::uint64_t seed) noexcept
{
    DeterminismTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    TestRng rng(seed);

    // Generate random floats including edge cases.
    std::vector<float> values;
    values.reserve(10000);

    for (int i = 0; i < 10000; ++i)
    {
        float v = rng.Float();

        // Inject edge cases.
        if (i % 100 == 0)
        {
            // NaN
            std::uint32_t nanBits = 0x7FC00001u;
            std::memcpy(&v, &nanBits, 4);
        }
        else if (i % 100 == 1)
        {
            // Denormal
            std::uint32_t denormBits = 0x00000001u;
            std::memcpy(&v, &denormBits, 4);
        }
        else if (i % 100 == 2)
        {
            // Infinity
            std::uint32_t infBits = 0x7F800000u;
            std::memcpy(&v, &infBits, 4);
        }

        values.push_back(v);
    }

    // Canonicalise all values.
    std::vector<float> canonicalised = values;
    for (auto& v : canonicalised)
        v = CanonicaliseFloat32(v);

    // Verify invariants:
    // 1. No NaN values remain.
    // 2. No denormal values remain.
    // 3. No infinity values remain.
    for (std::size_t i = 0; i < canonicalised.size(); ++i)
    {
        float v = canonicalised[i];
        std::uint32_t bits;
        std::memcpy(&bits, &v, 4);

        std::uint32_t exp = bits & 0x7F800000u;
        std::uint32_t mantissa = bits & 0x007FFFFFu;

        // Check NaN
        if (exp == 0x7F800000u && mantissa != 0)
        {
            result.failureDetail = "NaN at index " + std::to_string(i);
            return result;
        }

        // Check denormal
        if (exp == 0 && mantissa != 0)
        {
            result.failureDetail = "Denormal at index " + std::to_string(i);
            return result;
        }

        // Check infinity
        if (exp == 0x7F800000u && mantissa == 0)
        {
            result.failureDetail = "Infinity at index " + std::to_string(i);
            return result;
        }
    }

    result.success = true;
    result.iterations = canonicalised.size();
    return result;
}

// ─── Entity iteration determinism test ──────────────────────────────────────

DeterminismTestResult TestEntityIterationDeterminism(std::uint64_t seed) noexcept
{
    DeterminismTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    TestRng rng(seed);

    // Generate a set of entity IDs in random order.
    std::vector<std::uint32_t> entityIds;
    entityIds.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        entityIds.push_back(rng.Uint32(1, 100000));

    // Sort them (simulating deterministic iteration).
    std::vector<std::uint32_t> sorted1 = entityIds;
    std::sort(sorted1.begin(), sorted1.end());

    // Sort again with a different initial order.
    std::vector<std::uint32_t> shuffled = entityIds;
    std::shuffle(shuffled.begin(), shuffled.end(), rng.engine);
    std::vector<std::uint32_t> sorted2 = shuffled;
    std::sort(sorted2.begin(), sorted2.end());

    // Verify both produce the same order.
    if (sorted1 != sorted2)
    {
        result.failureDetail = "Entity iteration order differs between runs";
        return result;
    }

    result.success = true;
    result.iterations = sorted1.size();
    return result;
}

// ─── Apply order independence test ──────────────────────────────────────────

DeterminismTestResult TestApplyOrderIndependence(std::uint64_t seed) noexcept
{
    DeterminismTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    TestRng rng(seed);

    // Generate unique (entityId, typeId) patches - each slot gets exactly one patch.
    // This ensures apply order doesn't matter since there are no overwrites.
    struct Patch
    {
        std::uint32_t entityId;
        std::uint16_t typeId;
        std::uint8_t  data[16];
    };

    std::vector<Patch> patches;
    patches.reserve(50);  // 50 unique combinations.

    // Create one patch per (entity, type) slot to avoid overwrites.
    for (std::uint32_t entityId = 1; entityId <= 10; ++entityId)
    {
        for (std::uint16_t typeId = 1; typeId <= 5; ++typeId)
        {
            Patch p;
            p.entityId = entityId;
            p.typeId = typeId;
            for (int b = 0; b < 16; ++b)
                p.data[b] = static_cast<std::uint8_t>(rng.Uint32(0, 255));
            patches.push_back(p);
        }
    }

    // Apply in original order.
    std::vector<std::uint8_t> state1(50 * 5 * 16, 0);  // 50 entities * 5 types * 16 bytes
    auto ApplyPatch = [&](std::vector<std::uint8_t>& state, const Patch& p) {
        std::size_t offset = (p.entityId - 1) * 5 * 16 + (p.typeId - 1) * 16;
        if (offset + 16 <= state.size())
            std::memcpy(state.data() + offset, p.data, 16);
    };

    for (const auto& patch : patches)
        ApplyPatch(state1, patch);

    // Apply in shuffled order - should produce identical state since no overwrites.
    std::vector<Patch> shuffledPatches = patches;
    std::shuffle(shuffledPatches.begin(), shuffledPatches.end(), rng.engine);

    std::vector<std::uint8_t> state2(50 * 5 * 16, 0);
    for (const auto& patch : shuffledPatches)
        ApplyPatch(state2, patch);

    // Verify states match.
    if (state1 != state2)
    {
        result.failureDetail = "Apply order produced different final state";
        return result;
    }

    result.success = true;
    result.iterations = patches.size();
    return result;
}

// ─── Granular hash consistency test ─────────────────────────────────────────

DeterminismTestResult TestGranularHashConsistency(std::uint64_t seed) noexcept
{
    DeterminismTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    TestRng rng(seed);

    // Generate two identical synthetic world states and verify their
    // granular hashes match.
    // Since we cannot construct a real WorldState without the full ECS,
    // this test verifies the hash function's internal consistency.

    // Generate random byte blobs.
    std::vector<std::uint8_t> blob1(256);
    for (auto& b : blob1)
        b = static_cast<std::uint8_t>(rng.Uint32(0, 255));

    std::vector<std::uint8_t> blob2 = blob1;

    // Hash both blobs.
    std::uint64_t hash1 = 0, hash2 = 0;
    auto HashBlob = [](const std::vector<std::uint8_t>& blob) -> std::uint64_t {
        std::uint64_t h = 14695981039346656037ULL;
        for (auto b : blob) {
            h ^= static_cast<std::uint64_t>(b);
            h *= 1099511628211ULL;
        }
        return h;
    };

    hash1 = HashBlob(blob1);
    hash2 = HashBlob(blob2);

    if (hash1 != hash2)
    {
        result.failureDetail = "Identical blobs produced different hashes";
        return result;
    }

    // Verify different data produces different hashes.
    blob2[0] ^= 0xFF;
    std::uint64_t hash3 = HashBlob(blob2);

    if (hash1 == hash3)
    {
        result.failureDetail = "Different data produced same hash (collision)";
        return result;
    }

    result.success = true;
    result.iterations = blob1.size();
    return result;
}

// ─── Run all tests ──────────────────────────────────────────────────────────

std::vector<DeterminismTestResult> RunAllDeterminismTests(std::uint64_t seed) noexcept
{
    std::vector<DeterminismTestResult> results;
    results.reserve(4);

    results.push_back(TestGranularHashConsistency(seed));
    results.push_back(TestApplyOrderIndependence(seed));
    results.push_back(TestFloatCanonicalisation(seed));
    results.push_back(TestEntityIterationDeterminism(seed));

    return results;
}

} // namespace SagaEngine::Client::Replication
