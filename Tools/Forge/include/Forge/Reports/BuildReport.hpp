/// @file BuildReport.hpp
/// @brief Forge-local build report foundation contracts.

#pragma once

#include "Forge/Pipeline/BuildPlan.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace Forge::Reports
{

/// High-level state for an early Forge build report.
enum class BuildReportStatus
{
    Unknown,
    Planned,
    Blocked,
};

/// Minimal diagnostic severity vocabulary for Forge report foundation.
enum class BuildReportDiagnosticSeverity
{
    Info,
    Warning,
    Error,
    BuildBlocking,
};

/// One planned step captured in a report.
struct BuildReportStep
{
    std::string id;
    BuildStepKind kind = BuildStepKind::BuildBackend;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<std::string> dependencies;
    std::string toolName;
    BuildStepFailurePolicy failurePolicy = BuildStepFailurePolicy::StopOnFailure;
};

/// One structured diagnostic captured in a report.
struct BuildReportDiagnostic
{
    BuildReportDiagnosticSeverity severity = BuildReportDiagnosticSeverity::Info;
    std::string code;
    std::string title;
    std::string message;
    std::string stepId;
    std::string reference;
    std::map<std::string, std::string> metadata;
};

/// Small summary used by CLI, CI, and tests.
struct BuildReportSummary
{
    std::size_t stepCount = 0;
    std::size_t diagnosticCount = 0;
    std::size_t buildBlockingCount = 0;
};

/// Forge-local report shape. It can be mapped to shared contracts later.
struct BuildReport
{
    BuildReportStatus status = BuildReportStatus::Unknown;
    BuildReportSummary summary;
    std::vector<BuildReportStep> steps;
    std::vector<BuildReportDiagnostic> diagnostics;
    std::map<std::string, std::string> metadata;
};

} // namespace Forge::Reports
