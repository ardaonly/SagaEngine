/// @file StateCheckReport.hpp
/// @brief Deterministic SagaStateCheck JSON report writing.

#pragma once

#include "SagaStateCheck/StateSnapshot.hpp"

#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

namespace SagaStateCheck
{

struct StateCheckReportWriteDiagnostic
{
    std::string diagnosticId;
    std::string message;
    std::filesystem::path outputPath;
};

struct StateCheckReportWriteResult
{
    std::vector<StateCheckReportWriteDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty();
    }
};

struct StateCheckReportDocument
{
    std::string reportKind = "state_check_report";
    std::uint32_t schemaVersion = 1;
    StateSnapshot expected;
    StateSnapshot actual;
    DesyncReport desync;
    StateCheckOptions options;
};

[[nodiscard]] StateCheckReportDocument BuildStateCheckReport(
    StateSnapshot expected,
    StateSnapshot actual,
    const StateCheckOptions& options = {});

[[nodiscard]] StateCheckReportWriteResult WriteStateCheckReportJson(
    const StateCheckReportDocument& document,
    std::ostream& output);

[[nodiscard]] StateCheckReportWriteResult WriteStateCheckReportJsonToFile(
    const StateCheckReportDocument& document,
    const std::filesystem::path& outputPath);

} // namespace SagaStateCheck
