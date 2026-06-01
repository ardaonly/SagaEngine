/// @file CrashReportBuilder.hpp
/// @brief Builds crash and reliability reports from diagnostics snapshots.

#pragma once

#include <SagaEngine/Core/Log/LogEvent.hpp>
#include <SagaEngine/Diagnostics/Crash/CrashReport.hpp>
#include <SagaEngine/Diagnostics/Health/HealthSnapshot.hpp>
#include <SagaEngine/Diagnostics/Lifetime/LifetimeRecord.hpp>
#include <SagaEngine/Diagnostics/Memory/MemorySnapshot.hpp>
#include <SagaEngine/Diagnostics/Resource/ResourceSnapshot.hpp>

#include <cstdint>
#include <vector>

namespace SagaEngine::Diagnostics
{

/// Creates deterministic crash and reliability reports from captured diagnostics state.
class CrashReportBuilder
{
public:
    /// Build a crash/reliability report from immutable diagnostics snapshots.
    [[nodiscard]] static CrashReport Build(
        CrashContext context,
        std::uint64_t generationSequence,
        const HealthSnapshot& healthSnapshot,
        const std::vector<HealthRuleResult>& healthRuleResults,
        const std::vector<LifetimeRecord>& lifetimeRecords,
        const MemorySnapshot& memorySnapshot,
        const ResourceSnapshot& resourceSnapshot,
        const std::vector<Core::Log::LogEvent>& recentLogEvents);
};

} // namespace SagaEngine::Diagnostics
