/// @file SagaPublishReadiness.h
/// @brief Product-owned publish readiness gate and report writer.

#pragma once

#include <SagaShared/Diagnostics/DiagnosticPayload.hpp>
#include <SagaShared/Diagnostics/DiagnosticSummary.hpp>
#include <SagaShared/Publish/PublishBlocker.hpp>
#include <SagaShared/Publish/PublishReport.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace SagaProduct
{

/// One opaque external diagnostics report included in publish readiness.
struct SagaPublishDiagnosticsInput
{
    std::string key;
    std::filesystem::path path;
};

/// Product request for publish readiness validation.
struct SagaPublishReadinessRequest
{
    std::filesystem::path projectRoot;
    std::string profile = "shipping-full";
    std::filesystem::path reportPath;
    std::vector<SagaPublishDiagnosticsInput> diagnosticsInputs;
};

/// Product publish readiness outcome.
struct SagaPublishReadinessResult
{
    bool ok = false;
    std::filesystem::path reportPath;
    SagaShared::Publish::PublishReport report;
};

/// Product-owned publish readiness checks and JSON report writing.
class SagaPublishReadinessService
{
public:
    [[nodiscard]] SagaPublishReadinessResult Check(
        const SagaPublishReadinessRequest& request) const;

    [[nodiscard]] static std::filesystem::path DefaultReportPath(
        const std::filesystem::path& projectRoot);

    [[nodiscard]] static std::vector<SagaPublishDiagnosticsInput>
        ParseDiagnosticsInputs(const std::vector<std::string>& specs,
                               std::string& error);
};

[[nodiscard]] const char* ToString(
    SagaShared::Publish::PublishStatus status) noexcept;

[[nodiscard]] const char* ToString(
    SagaShared::Publish::PublishBlockerKind kind) noexcept;

} // namespace SagaProduct
