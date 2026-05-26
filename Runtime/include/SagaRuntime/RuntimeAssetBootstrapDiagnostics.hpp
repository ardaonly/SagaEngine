/// @file RuntimeAssetBootstrapDiagnostics.hpp
/// @brief Runtime-owned package asset bootstrap diagnostic classification.

#pragma once

#include <SagaRuntime/RuntimeAssetBootstrap.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaRuntime
{

/// Runtime asset bootstrap report state.
enum class RuntimeAssetBootstrapReportState
{
    Ready = 0,
    Blocked,
    Empty,
};

/// Summary of a Runtime asset bootstrap result.
struct RuntimeAssetBootstrapReportSummary
{
    RuntimeAssetBootstrapReportState state =
        RuntimeAssetBootstrapReportState::Ready;
    bool succeeded = true;                         ///< True when bootstrap result has no diagnostics.
    std::size_t registeredAssetCount = 0;          ///< Number of assets registered by bootstrap.
    std::size_t diagnosticCount = 0;               ///< Total diagnostics in the bootstrap result.
    std::size_t errorCount = 0;                    ///< Error diagnostics in the bootstrap result.
    std::size_t warningCount = 0;                  ///< Warning diagnostics in the bootstrap result.
    std::filesystem::path packageManifestPath;     ///< Package manifest path when options are supplied.
    std::filesystem::path packageBaseDirectory;    ///< Package base directory when options are supplied.
};

/// Classified view over one Runtime asset bootstrap diagnostic.
struct RuntimeAssetBootstrapDiagnosticView
{
    RuntimeAssetBootstrapDiagnosticSeverity severity =
        RuntimeAssetBootstrapDiagnosticSeverity::Error;
    RuntimeAssetBootstrapDiagnosticCategory category =
        RuntimeAssetBootstrapDiagnosticCategory::StartupValidation;
    bool blocking = true;                          ///< True when this diagnostic blocks bootstrap success.

    std::string diagnosticId;
    std::string message;
    std::filesystem::path manifestPath;
    std::optional<std::string> fieldName;
    std::optional<std::size_t> referenceIndex;
    std::optional<std::size_t> itemIndex;
    std::optional<std::string> resourceId;
    std::optional<SagaEngine::Resources::AssetId> assetId;
    std::optional<std::filesystem::path> resolvedPath;
};

/// Runtime-owned asset bootstrap result classifier.
class RuntimeAssetBootstrapDiagnostics
{
public:
    /// Summarize bootstrap state without app logging or process policy.
    [[nodiscard]] static RuntimeAssetBootstrapReportSummary Summarize(
        const RuntimeAssetBootstrapResult& result);

    /// Summarize bootstrap state and preserve the options used by the caller.
    [[nodiscard]] static RuntimeAssetBootstrapReportSummary Summarize(
        const RuntimeAssetBootstrapResult& result,
        const RuntimeAssetBootstrapOptions& options);

    /// Build classified diagnostic views for app-owned logging.
    [[nodiscard]] static std::vector<RuntimeAssetBootstrapDiagnosticView>
    BuildDiagnosticViews(const RuntimeAssetBootstrapResult& result);
};

} // namespace SagaRuntime
