/// @file RuntimeStartupDiagnostics.hpp
/// @brief Runtime-owned startup report diagnostic classification.

#pragma once

#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaRuntime
{

/// Runtime startup diagnostic severity.
enum class RuntimeStartupDiagnosticSeverity
{
    Info = 0,
    Warning,
    Error,
};

/// Runtime startup diagnostic category.
enum class RuntimeStartupDiagnosticCategory
{
    StartupPreflight = 0,
    StartupSession,
};

/// Runtime startup report state.
enum class RuntimeStartupReportState
{
    Ready = 0,
    PreflightSkipped,
    Blocked,
};

/// Classified view over one startup diagnostic.
struct RuntimeStartupDiagnosticView
{
    RuntimeStartupDiagnosticSeverity severity =
        RuntimeStartupDiagnosticSeverity::Error;
    RuntimeStartupDiagnosticCategory category =
        RuntimeStartupDiagnosticCategory::StartupPreflight;
    bool blocking = true;

    std::string diagnosticId;
    std::string message;
    std::filesystem::path manifestPath;
    std::optional<std::string> fieldName;
    std::optional<std::size_t> referenceIndex;
    std::optional<std::size_t> itemIndex;
    std::optional<std::string> resourceId;
    std::optional<std::filesystem::path> resolvedPath;
};

/// Summary of a Runtime startup report.
struct RuntimeStartupReportSummary
{
    RuntimeStartupReportState state = RuntimeStartupReportState::Ready;
    bool startupAllowed = true;
    bool preflightSkipped = false;
    std::size_t diagnosticCount = 0;
    std::size_t blockingDiagnosticCount = 0;
};

/// Runtime-owned startup report classifier.
class RuntimeStartupDiagnostics
{
public:
    /// Summarize startup state without owning app logging or exit policy.
    [[nodiscard]] static RuntimeStartupReportSummary Summarize(
        const RuntimeStartupReport& report);

    /// Build classified diagnostic views for app-owned logging.
    [[nodiscard]] static std::vector<RuntimeStartupDiagnosticView>
    BuildDiagnosticViews(const RuntimeStartupReport& report);
};

} // namespace SagaRuntime
