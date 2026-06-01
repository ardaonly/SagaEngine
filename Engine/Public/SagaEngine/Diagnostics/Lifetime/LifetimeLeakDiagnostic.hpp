/// @file LifetimeLeakDiagnostic.hpp
/// @brief Defines classified lifetime leak candidates and summaries.

#pragma once

#include <SagaEngine/Diagnostics/Lifetime/LifetimeRecord.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SagaEngine::Diagnostics
{

/// Active lifetime record classified as a leak candidate.
struct LifetimeLeakCandidate
{
    std::uint64_t objectId = 0;       ///< Diagnostics-local object identifier.
    std::uint64_t externalId = 0;     ///< Optional engine/server identifier.
    std::uint64_t ownerObjectId = 0;  ///< Optional tracked owner object id.
    std::string typeName;             ///< Logical leaked object type.
    std::string ownerSystem;          ///< System that created the object.
    std::uint64_t createdTick = 0;    ///< Creation tick when available.
};

/// Deterministic summary of active lifetime leak candidates.
struct LifetimeLeakSummary
{
    std::size_t totalActive = 0;                  ///< Total active leak candidates.
    std::map<std::string, std::size_t> byType;    ///< Counts grouped by typeName.
    std::map<std::string, std::size_t> byOwnerSystem; ///< Counts grouped by ownerSystem.
};

/// Classified active lifetime leak candidates and grouped counts.
struct LifetimeLeakDiagnostic
{
    std::vector<LifetimeLeakCandidate> candidates;
    LifetimeLeakSummary summary;
};

/// Build deterministic lifetime leak diagnostics from active records.
[[nodiscard]] LifetimeLeakDiagnostic BuildLifetimeLeakDiagnostic(
    const std::vector<LifetimeRecord>& records);

} // namespace SagaEngine::Diagnostics
