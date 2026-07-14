/// @file SchemaMigrationTest.h
/// @brief Schema migration and backward compatibility tests.
///
/// Purpose: Verifies that the schema version negotiation and migration
///          policy correctly handles version mismatches, component
///          deprecation, and forward-compatible type skipping.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

struct MigrationTestResult
{
    bool              success = false;
    std::string       failureDetail;
};

/// Test major version mismatch (incompatible).
[[nodiscard]] MigrationTestResult TestMajorMismatch() noexcept;

/// Test minor version forward compatibility (server newer).
[[nodiscard]] MigrationTestResult TestMinorForwardCompat() noexcept;

/// Test minor version backward compatibility (client newer).
[[nodiscard]] MigrationTestResult TestMinorBackwardCompat() noexcept;

/// Test patch version negotiation.
[[nodiscard]] MigrationTestResult TestPatchNegotiation() noexcept;

/// Test component type skipping for unknown types.
[[nodiscard]] MigrationTestResult TestComponentSkip() noexcept;

/// Run all schema migration tests.
[[nodiscard]] std::vector<MigrationTestResult> RunAllMigrationTests() noexcept;

} // namespace SagaEngine::Client::Replication
