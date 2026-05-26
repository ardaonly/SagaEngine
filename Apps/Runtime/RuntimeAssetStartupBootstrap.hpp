/// @file RuntimeAssetStartupBootstrap.hpp
/// @brief App-local Runtime asset bootstrap helper seam.

#pragma once

#include <SagaRuntime/RuntimeAssetBootstrapDiagnostics.hpp>
#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <SagaEngine/Resources/AssetRegistry.h>

#include <vector>

namespace SagaRuntimeApp
{

/// App-facing result for startup asset bootstrap policy and logging.
struct RuntimeAssetStartupBootstrapResult
{
    bool bootstrapSkipped = false;
    SagaRuntime::RuntimeAssetBootstrapResult bootstrap;
    SagaRuntime::RuntimeAssetBootstrapReportSummary summary;
    std::vector<SagaRuntime::RuntimeAssetBootstrapDiagnosticView> diagnostics;

    /// Return true when bootstrap succeeded or was intentionally skipped.
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return bootstrapSkipped || summary.succeeded;
    }
};

/// Bootstrap package assets into a caller-owned registry when a package exists.
class RuntimeAssetStartupBootstrap
{
public:
    [[nodiscard]] static RuntimeAssetStartupBootstrapResult Bootstrap(
        const SagaRuntime::RuntimeSessionDescriptor& descriptor,
        SagaEngine::Resources::AssetRegistry& registry);
};

} // namespace SagaRuntimeApp
