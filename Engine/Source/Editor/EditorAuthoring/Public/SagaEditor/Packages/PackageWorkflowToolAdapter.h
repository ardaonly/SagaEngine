// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Packages/PackageAuthoring.h"
#include "SagaEditor/Processes/ManagedToolRuntime.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace SagaEditor
{
class EditorDocumentWorkspace;

struct PackageToolOperationContext final
{
    std::shared_ptr<std::atomic_bool> cancellation;
    std::function<void(std::string_view)> progress;

    [[nodiscard]] bool CancellationRequested() const noexcept
    {
        return cancellation && cancellation->load(std::memory_order_relaxed);
    }
};

struct PackageInspectionResult final
{
    bool passed = false;
    std::string code;
    std::string message;
    std::string inspectionBytes;
};

/// Qt-free process boundary used by the package workflow. Implementations may
/// execute tools, but never publish canonical project inputs themselves.
class IPackageWorkflowToolAdapter
{
public:
    virtual ~IPackageWorkflowToolAdapter() = default;
    [[nodiscard]] virtual std::pair<PackageWorkflowCapture, PackageWorkflowResult>
        Capture(const EditorDocumentWorkspace& workspace) const = 0;
    [[nodiscard]] virtual PackageCandidateResolution ResolveCandidate(
        const PackageWorkflowCapture& capture,
        const std::filesystem::path& stagedManifest,
        const std::filesystem::path& stagingRoot,
        const PackageToolOperationContext& context) const = 0;
    [[nodiscard]] virtual PackageUsageAnalysis AnalyzeUsage(
        const PackageWorkflowCapture& capture, std::string_view packageId,
        std::string_view module, const std::filesystem::path& stagingRoot,
        const PackageToolOperationContext& context) const = 0;
    [[nodiscard]] virtual PackageInspectionResult RefreshGeneration(
        const PackageWorkflowCapture& capture,
        const PackageToolOperationContext& context) const = 0;
    [[nodiscard]] virtual PackageInspectionResult InspectCanonical(
        const PackageWorkflowCapture& capture,
        const PackageToolOperationContext& context) const = 0;
};

struct ManagedPackageWorkflowToolchain final
{
    ManagedToolRuntimeOptions managedRuntime;
    std::filesystem::path engineRoot;
    std::filesystem::path runtimeBridge;
    std::chrono::milliseconds timeout{std::chrono::minutes(2)};
};

/// Product adapter for the canonical SagaPackager and SagaScript executables.
/// Every invocation goes through ManagedToolRuntime so PATH and cwd are never
/// executable authorities.
class ManagedPackageWorkflowToolAdapter final : public IPackageWorkflowToolAdapter
{
public:
    explicit ManagedPackageWorkflowToolAdapter(
        ManagedPackageWorkflowToolchain toolchain = {});
    [[nodiscard]] std::pair<PackageWorkflowCapture, PackageWorkflowResult>
        Capture(const EditorDocumentWorkspace& workspace) const override;
    [[nodiscard]] PackageCandidateResolution ResolveCandidate(
        const PackageWorkflowCapture& capture,
        const std::filesystem::path& stagedManifest,
        const std::filesystem::path& stagingRoot,
        const PackageToolOperationContext& context) const override;
    [[nodiscard]] PackageUsageAnalysis AnalyzeUsage(
        const PackageWorkflowCapture& capture, std::string_view packageId,
        std::string_view module, const std::filesystem::path& stagingRoot,
        const PackageToolOperationContext& context) const override;
    [[nodiscard]] PackageInspectionResult RefreshGeneration(
        const PackageWorkflowCapture& capture,
        const PackageToolOperationContext& context) const override;
    [[nodiscard]] PackageInspectionResult InspectCanonical(
        const PackageWorkflowCapture& capture,
        const PackageToolOperationContext& context) const override;

private:
    [[nodiscard]] PackageInspectionResult RunPackager(
        std::vector<std::string> arguments,
        const std::filesystem::path& report,
        const PackageToolOperationContext& context,
        std::string_view failureCode) const;
    ManagedPackageWorkflowToolchain toolchain_;
};

[[nodiscard]] ManagedPackageWorkflowToolchain
    DiscoverManagedPackageWorkflowToolchain();
}
