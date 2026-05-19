/// @file DiagnosticSummary.hpp
/// @brief Shared diagnostic count and severity summary.

#pragma once

#include "SagaShared/Diagnostics/DiagnosticPayload.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"

#include <cstdint>

namespace SagaShared::Diagnostics
{

/// Count summary for a collection of diagnostic payloads.
struct DiagnosticSummary
{
    std::uint32_t totalCount = 0;
    std::uint32_t traceCount = 0;
    std::uint32_t infoCount = 0;
    std::uint32_t warningCount = 0;
    std::uint32_t errorCount = 0;
    std::uint32_t fatalCount = 0;
    std::uint32_t buildBlockingCount = 0;
    std::uint32_t publishBlockingCount = 0;
    std::uint32_t securityCount = 0;
    DiagnosticSeverity highestSeverity = DiagnosticSeverity::Trace;

    void Add(DiagnosticSeverity severity) noexcept
    {
        ++totalCount;

        switch (severity)
        {
            case DiagnosticSeverity::Trace: ++traceCount; break;
            case DiagnosticSeverity::Info: ++infoCount; break;
            case DiagnosticSeverity::Warning: ++warningCount; break;
            case DiagnosticSeverity::Error: ++errorCount; break;
            case DiagnosticSeverity::Fatal: ++fatalCount; break;
            case DiagnosticSeverity::BuildBlocking: ++buildBlockingCount; break;
            case DiagnosticSeverity::PublishBlocking: ++publishBlockingCount; break;
            case DiagnosticSeverity::Security: ++securityCount; break;
        }

        if (DiagnosticSeverityRank(severity) > DiagnosticSeverityRank(highestSeverity))
        {
            highestSeverity = severity;
        }
    }

    void Add(const DiagnosticPayload& payload) noexcept
    {
        Add(payload.severity);
    }

    [[nodiscard]] std::uint32_t Count(DiagnosticSeverity severity) const noexcept
    {
        switch (severity)
        {
            case DiagnosticSeverity::Trace: return traceCount;
            case DiagnosticSeverity::Info: return infoCount;
            case DiagnosticSeverity::Warning: return warningCount;
            case DiagnosticSeverity::Error: return errorCount;
            case DiagnosticSeverity::Fatal: return fatalCount;
            case DiagnosticSeverity::BuildBlocking: return buildBlockingCount;
            case DiagnosticSeverity::PublishBlocking: return publishBlockingCount;
            case DiagnosticSeverity::Security: return securityCount;
        }

        return 0;
    }
};

} // namespace SagaShared::Diagnostics
