/// @file LifetimeLeakDiagnostic.cpp
/// @brief Implements classified lifetime leak diagnostics.

#include <SagaEngine/Diagnostics/Lifetime/LifetimeLeakDiagnostic.hpp>

#include <algorithm>
#include <utility>

namespace SagaEngine::Diagnostics
{
namespace
{

[[nodiscard]] LifetimeLeakCandidate ToCandidate(const LifetimeRecord& record)
{
    LifetimeLeakCandidate candidate;
    candidate.objectId = record.objectId;
    candidate.externalId = record.externalId;
    candidate.ownerObjectId = record.ownerObjectId;
    candidate.typeName = record.typeName;
    candidate.ownerSystem = record.ownerSystem;
    candidate.createdTick = record.createdTick;
    return candidate;
}

} // namespace

LifetimeLeakDiagnostic BuildLifetimeLeakDiagnostic(
    const std::vector<LifetimeRecord>& records)
{
    LifetimeLeakDiagnostic diagnostic;
    for (const auto& record : records)
    {
        if (record.destroyed)
        {
            continue;
        }

        auto candidate = ToCandidate(record);
        ++diagnostic.summary.byType[candidate.typeName];
        ++diagnostic.summary.byOwnerSystem[candidate.ownerSystem];
        diagnostic.candidates.push_back(std::move(candidate));
    }

    std::sort(diagnostic.candidates.begin(),
              diagnostic.candidates.end(),
              [](const LifetimeLeakCandidate& lhs,
                 const LifetimeLeakCandidate& rhs) {
                  return lhs.objectId < rhs.objectId;
              });
    diagnostic.summary.totalActive = diagnostic.candidates.size();
    return diagnostic;
}

} // namespace SagaEngine::Diagnostics
