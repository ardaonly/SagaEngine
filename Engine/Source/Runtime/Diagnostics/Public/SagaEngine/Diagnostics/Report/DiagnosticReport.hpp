/// @file DiagnosticReport.hpp
/// @brief Defines Engine-owned diagnostics report payloads.

#pragma once

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Diagnostics/Fault/FaultReport.hpp>
#include <SagaEngine/Diagnostics/Memory/MemoryRecord.hpp>
#include <SagaEngine/Diagnostics/Resource/ResourceRecord.hpp>
#include <SagaEngine/Diagnostics/Resource/ResourceSnapshot.hpp>
#include <SagaEngine/Diagnostics/ServerLifecycle/ServerLifecycleTracker.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::Diagnostics
{

namespace DiagnosticReportKinds {

inline constexpr const char* HealthSnapshot = "health_snapshot";
inline constexpr const char* LifetimeLeaks = "lifetime_leaks";
inline constexpr const char* OperationalSnapshot = "operational_snapshot";

} // namespace DiagnosticReportKinds

/// Numeric health metric captured into a diagnostics report.
struct DiagnosticReportHealthMetric
{
    std::string name;
    std::string type;
    double value = 0.0;
};

/// Alive lifetime record captured as a leak candidate.
struct DiagnosticReportLifetimeLeak
{
    std::uint64_t objectId = 0;
    std::uint64_t externalId = 0;
    std::uint64_t ownerObjectId = 0;
    std::string typeName;
    std::string ownerSystem;
    std::uint64_t createdTick = 0;
};

/// Recent structured log event captured into a diagnostics report.
struct DiagnosticReportLogEvent
{
    Core::Log::Level level = Core::Log::Level::Info;
    std::string tag;
    std::string message;
    std::vector<std::pair<std::string, std::string>> context;
};

/// Summary counts for a diagnostics report.
struct DiagnosticReportSummary
{
    std::size_t healthMetricCount = 0;
    std::size_t lifetimeLeakCount = 0;
    std::size_t memorySystemCount = 0;
    std::size_t activeResourceCount = 0;
    std::size_t recentLogEventCount = 0;
    std::size_t faultReportCount = 0;
};

/// Engine-owned diagnostics report model for local operational snapshots.
struct DiagnosticReport
{
    std::uint32_t schemaVersion = 1;
    std::string reportKind;
    std::uint64_t generationSequence = 0;
    DiagnosticReportSummary summary;
    std::vector<DiagnosticReportHealthMetric> healthMetrics;
    std::vector<DiagnosticReportLifetimeLeak> lifetimeLeaks;
    std::vector<MemoryRecord> memoryRecords;
    ResourceLeakSummary resourceSummary;
    std::vector<ResourceRecord> activeResources;
    std::vector<DiagnosticReportLogEvent> recentLogEvents;
    ServerLifecycleSnapshot serverLifecycle;
    FaultSnapshot faults;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaEngine::Diagnostics
