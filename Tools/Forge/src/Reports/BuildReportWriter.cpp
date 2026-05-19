/// @file BuildReportWriter.cpp
/// @brief Deterministic JSON writer for Forge build reports.

#include "Forge/Reports/BuildReportWriter.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace Forge::Reports
{

namespace
{

std::string JsonEscape(const std::string& value)
{
    std::ostringstream os;
    for (const unsigned char ch : value)
    {
        switch (ch)
        {
            case '"':
                os << "\\\"";
                break;
            case '\\':
                os << "\\\\";
                break;
            case '\b':
                os << "\\b";
                break;
            case '\f':
                os << "\\f";
                break;
            case '\n':
                os << "\\n";
                break;
            case '\r':
                os << "\\r";
                break;
            case '\t':
                os << "\\t";
                break;
            default:
                if (ch < 0x20)
                {
                    os << "\\u"
                       << std::hex << std::uppercase
                       << std::setw(4) << std::setfill('0')
                       << static_cast<int>(ch)
                       << std::dec << std::nouppercase;
                }
                else
                {
                    os << static_cast<char>(ch);
                }
                break;
        }
    }
    return os.str();
}

void WriteString(std::ostream& os, const std::string& value)
{
    os << '"' << JsonEscape(value) << '"';
}

std::string ToString(const BuildReportStatus status)
{
    switch (status)
    {
        case BuildReportStatus::Unknown:
            return "Unknown";
        case BuildReportStatus::Planned:
            return "Planned";
        case BuildReportStatus::Blocked:
            return "Blocked";
    }

    return "Unknown";
}

std::string ToString(const BuildReportDiagnosticSeverity severity)
{
    switch (severity)
    {
        case BuildReportDiagnosticSeverity::Info:
            return "Info";
        case BuildReportDiagnosticSeverity::Warning:
            return "Warning";
        case BuildReportDiagnosticSeverity::Error:
            return "Error";
        case BuildReportDiagnosticSeverity::BuildBlocking:
            return "BuildBlocking";
    }

    return "Info";
}

std::string ToString(const BuildStepKind kind)
{
    switch (kind)
    {
        case BuildStepKind::InstallDependencies:
            return "InstallDependencies";
        case BuildStepKind::ConfigureBackend:
            return "ConfigureBackend";
        case BuildStepKind::BuildBackend:
            return "BuildBackend";
        case BuildStepKind::RunTests:
            return "RunTests";
        case BuildStepKind::InstallTarget:
            return "InstallTarget";
    }

    return "BuildBackend";
}

std::string ToString(const BuildStepFailurePolicy policy)
{
    switch (policy)
    {
        case BuildStepFailurePolicy::StopOnFailure:
            return "StopOnFailure";
        case BuildStepFailurePolicy::ContinueOnFailure:
            return "ContinueOnFailure";
    }

    return "StopOnFailure";
}

void WriteStringArray(std::ostream& os, const std::vector<std::string>& values)
{
    os << "[";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
        {
            os << ", ";
        }
        WriteString(os, values[i]);
    }
    os << "]";
}

void WriteStringMap(std::ostream& os,
                    const std::map<std::string, std::string>& values,
                    const std::string& indent)
{
    os << "{";
    if (!values.empty())
    {
        os << "\n";
        std::size_t index = 0;
        for (const auto& [key, value] : values)
        {
            os << indent;
            WriteString(os, key);
            os << ": ";
            WriteString(os, value);
            os << (++index == values.size() ? "\n" : ",\n");
        }
    }
    os << (values.empty() ? "" : indent.substr(0, indent.size() - 2)) << "}";
}

void WriteStep(std::ostream& os, const BuildReportStep& step)
{
    os << "    {\n";
    os << "      \"id\": ";
    WriteString(os, step.id);
    os << ",\n      \"kind\": ";
    WriteString(os, ToString(step.kind));
    os << ",\n      \"tool\": ";
    WriteString(os, step.toolName);
    os << ",\n      \"failurePolicy\": ";
    WriteString(os, ToString(step.failurePolicy));
    os << ",\n      \"inputs\": ";
    WriteStringArray(os, step.inputs);
    os << ",\n      \"outputs\": ";
    WriteStringArray(os, step.outputs);
    os << ",\n      \"dependencies\": ";
    WriteStringArray(os, step.dependencies);
    os << "\n    }";
}

void WriteDiagnostic(std::ostream& os, const BuildReportDiagnostic& diagnostic)
{
    os << "    {\n";
    os << "      \"severity\": ";
    WriteString(os, ToString(diagnostic.severity));
    os << ",\n      \"code\": ";
    WriteString(os, diagnostic.code);
    os << ",\n      \"title\": ";
    WriteString(os, diagnostic.title);
    os << ",\n      \"message\": ";
    WriteString(os, diagnostic.message);
    os << ",\n      \"stepId\": ";
    WriteString(os, diagnostic.stepId);
    os << ",\n      \"reference\": ";
    WriteString(os, diagnostic.reference);
    os << ",\n      \"metadata\": ";
    WriteStringMap(os, diagnostic.metadata, "        ");
    os << "\n    }";
}

} // namespace

void BuildReportWriter::WriteJson(const BuildReport& report, std::ostream& os)
{
    os << "{\n";
    os << "  \"status\": ";
    WriteString(os, ToString(report.status));
    os << ",\n  \"summary\": {\n";
    os << "    \"stepCount\": " << report.summary.stepCount << ",\n";
    os << "    \"diagnosticCount\": " << report.summary.diagnosticCount << ",\n";
    os << "    \"buildBlockingCount\": " << report.summary.buildBlockingCount << "\n";
    os << "  },\n";

    os << "  \"steps\": [";
    if (!report.steps.empty())
    {
        os << "\n";
        for (std::size_t i = 0; i < report.steps.size(); ++i)
        {
            WriteStep(os, report.steps[i]);
            os << (i + 1 == report.steps.size() ? "\n" : ",\n");
        }
        os << "  ";
    }
    os << "],\n";

    os << "  \"diagnostics\": [";
    if (!report.diagnostics.empty())
    {
        os << "\n";
        for (std::size_t i = 0; i < report.diagnostics.size(); ++i)
        {
            WriteDiagnostic(os, report.diagnostics[i]);
            os << (i + 1 == report.diagnostics.size() ? "\n" : ",\n");
        }
        os << "  ";
    }
    os << "],\n";

    os << "  \"metadata\": ";
    WriteStringMap(os, report.metadata, "    ");
    os << "\n}\n";
}

} // namespace Forge::Reports
