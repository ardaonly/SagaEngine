/// @file DiagnosticReportWriter.hpp
/// @brief Writes diagnostics reports to deterministic JSON.

#pragma once

#include <SagaEngine/Diagnostics/Crash/CrashReport.hpp>
#include <SagaEngine/Diagnostics/Report/DiagnosticReport.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace SagaEngine::Diagnostics
{

/// Structured diagnostic emitted while writing a diagnostics report.
struct DiagnosticReportWriteDiagnostic
{
    std::string diagnosticId;
    std::string message;
    std::optional<std::filesystem::path> outputPath;
};

/// Result of writing a diagnostics report.
struct DiagnosticReportWriteResult
{
    std::size_t bytesWritten = 0;
    std::vector<DiagnosticReportWriteDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

/// Deterministic JSON writer for Engine diagnostics reports.
class DiagnosticReportWriter
{
public:
    [[nodiscard]] static DiagnosticReportWriteResult WriteJson(
        const DiagnosticReport& report,
        std::ostream& output);
    [[nodiscard]] static DiagnosticReportWriteResult WriteJson(
        const CrashReport& report,
        std::ostream& output);

    [[nodiscard]] static DiagnosticReportWriteResult WriteJsonToFile(
        const DiagnosticReport& report,
        const std::filesystem::path& outputPath);
    [[nodiscard]] static DiagnosticReportWriteResult WriteJsonToFile(
        const CrashReport& report,
        const std::filesystem::path& outputPath);
};

namespace DiagnosticReportWriterDiagnostics {

inline constexpr const char* OutputDirectoryFailed =
    "Diagnostics.DiagnosticReportWriter.OutputDirectoryFailed";
inline constexpr const char* OutputOpenFailed =
    "Diagnostics.DiagnosticReportWriter.OutputOpenFailed";
inline constexpr const char* OutputWriteFailed =
    "Diagnostics.DiagnosticReportWriter.OutputWriteFailed";

} // namespace DiagnosticReportWriterDiagnostics

} // namespace SagaEngine::Diagnostics
