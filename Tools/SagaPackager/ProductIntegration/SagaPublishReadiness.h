/// @file SagaPublishReadiness.h
/// @brief Product-owned publish readiness gate and report writer.

#pragma once

#include <SagaShared/Diagnostics/DiagnosticPayload.hpp>
#include <SagaShared/Diagnostics/DiagnosticSummary.hpp>
#include <SagaShared/Publish/PublishBlocker.hpp>
#include <SagaShared/Publish/PublishReport.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
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

/// Loader diagnostic captured for package/asset identity coverage reporting.
struct SagaPublishPackageAssetDiagnostic
{
    std::string code;
    std::string message;
    std::filesystem::path path;
    std::optional<std::size_t> referenceIndex;
    std::optional<std::size_t> assetIndex;
    std::optional<std::size_t> mappingIndex;
    std::optional<std::string> field;
};

/// Coverage record for one package-referenced runtime asset manifest.
struct SagaPublishAssetManifestCoverage
{
    std::string id;
    std::filesystem::path path;
    std::filesystem::path resolvedPath;
    bool exists = false;
    bool loads = false;
    std::size_t assetCount = 0;
    std::vector<SagaPublishPackageAssetDiagnostic> diagnostics;
};

/// Coverage record for one package-referenced asset identity manifest.
struct SagaPublishAssetIdentityManifestCoverage
{
    bool referenced = false;
    std::filesystem::path path;
    std::filesystem::path resolvedPath;
    bool exists = false;
    bool loads = false;
    std::size_t mappingCount = 0;
    std::vector<SagaPublishPackageAssetDiagnostic> diagnostics;
};

/// Product-local package asset coverage serialized into publish reports.
struct SagaPublishPackageAssetIdentityCoverage
{
    std::string packageSlot;
    std::filesystem::path packageManifestPath;
    bool packageManifestExists = false;
    bool packageManifestLoads = false;
    std::string packageId;
    std::string packageKind;
    SagaPublishAssetIdentityManifestCoverage assetIdentityManifest;
    std::vector<SagaPublishAssetManifestCoverage> assetManifests;
    std::vector<SagaPublishPackageAssetDiagnostic> diagnostics;
};

/// Product publish readiness outcome.
struct SagaPublishReadinessResult
{
    bool ok = false;
    std::filesystem::path reportPath;
    SagaShared::Publish::PublishReport report;
    std::vector<SagaPublishPackageAssetIdentityCoverage>
        packageAssetIdentityCoverage;
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
