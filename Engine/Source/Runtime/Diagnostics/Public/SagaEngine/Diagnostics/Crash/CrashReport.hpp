/// @file CrashReport.hpp
/// @brief Defines crash context and reliability diagnostics reports.

#pragma once

#include <SagaEngine/Diagnostics/Crash/CrashContext.hpp>
#include <SagaEngine/Diagnostics/Health/HealthRuleResult.hpp>
#include <SagaEngine/Diagnostics/Lifetime/LifetimeLeakDiagnostic.hpp>
#include <SagaEngine/Diagnostics/Report/DiagnosticReport.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace SagaEngine::Diagnostics
{

/// Summary counts for a crash or reliability report.
struct CrashReportSummary
{
    std::size_t healthMetricCount = 0;          ///< Number of health metrics captured.
    std::size_t healthRuleViolationCount = 0;   ///< Number of evaluated failing health rules.
    std::size_t lifetimeLeakCount = 0;          ///< Number of active lifetime leak candidates.
    std::size_t memorySystemCount = 0;          ///< Number of explicit memory systems captured.
    std::size_t activeResourceCount = 0;        ///< Number of active resource candidates.
    std::size_t recentLogEventCount = 0;        ///< Number of recent log events captured.
};

/// Engine-owned crash context and reliability report model.
struct CrashReport
{
    std::uint32_t schemaVersion = 1;                    ///< Crash report schema version.
    std::string reportKind;                             ///< Stable crash/reliability report kind.
    std::string reason;                                 ///< Human-readable report reason.
    std::uint64_t generationSequence = 0;               ///< Deterministic local sequence number.
    std::optional<std::string> threadId;                ///< Optional captured thread id.
    CrashReportSummary summary;                         ///< Aggregate report counts.
    std::vector<DiagnosticReportHealthMetric> healthMetrics; ///< Captured health metrics.
    std::vector<HealthRuleResult> healthRuleViolations; ///< Evaluated failing health rules.
    std::vector<LifetimeLeakCandidate> lifetimeLeaks;   ///< Active lifetime leak candidates.
    LifetimeLeakSummary lifetimeLeakSummary;            ///< Leak counts by type and owner.
    std::vector<MemoryRecord> memoryRecords;             ///< Explicit memory snapshot records.
    ResourceLeakSummary resourceSummary;                 ///< Active resource counts.
    std::vector<ResourceRecord> activeResources;         ///< Active resource leak candidates.
    std::vector<DiagnosticReportLogEvent> recentLogEvents; ///< Captured recent log events.
    std::map<std::string, std::string> metadata;        ///< Sorted report context fields.
};

} // namespace SagaEngine::Diagnostics
