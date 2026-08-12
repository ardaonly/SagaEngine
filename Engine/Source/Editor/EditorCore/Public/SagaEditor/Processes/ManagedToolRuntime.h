// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Processes/ToolProcessRunner.h"

#include <map>
#include <optional>

namespace SagaEditor
{

enum class ManagedToolId
{
    ProjectKit,
    Script,
    Packager,
};

enum class ManagedToolResolutionMode
{
    Unresolved,
    Installed,
    TestStage,
    EnvironmentStage,
    BuildStage,
    DeveloperHost,
};

struct ManagedToolDescriptor
{
    ManagedToolId id = ManagedToolId::ProjectKit;
    ManagedToolResolutionMode resolutionMode = ManagedToolResolutionMode::Unresolved;
    std::filesystem::path trustedRoot;
    std::filesystem::path manifest;
    std::filesystem::path runtimeRoot;
    std::filesystem::path host;
    std::filesystem::path toolRoot;
    std::filesystem::path dll;
    std::filesystem::path runtimeConfig;
    std::filesystem::path deps;
    std::string framework;
    std::string minimumVersion;
    std::string runtimeVersion;
    std::string rid;
};

struct ManagedToolDiagnostic
{
    std::string code;
    std::string message;
    std::filesystem::path path;
    std::map<std::string, std::string> metadata;
};

struct ManagedToolInvocation
{
    ManagedToolDescriptor descriptor;
    std::vector<ManagedToolDiagnostic> diagnostics;
    std::optional<ToolProcessRequest> process;

    [[nodiscard]] bool Ready() const noexcept
    {
        return diagnostics.empty() && process.has_value();
    }
};

struct ManagedToolProcessResult
{
    ManagedToolDescriptor descriptor;
    std::vector<ManagedToolDiagnostic> preflightDiagnostics;
    ToolProcessResult process;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return preflightDiagnostics.empty() && process.Succeeded();
    }
};

struct ManagedToolRuntimeOptions
{
    std::filesystem::path productExecutable;
    std::filesystem::path testStage;
    std::filesystem::path developerDotnetHost;
    /// When populated, every invocation verifies this complete relative-path
    /// inventory before launching the managed host.
    std::map<std::filesystem::path, std::string> expectedInventory;
};

class ManagedToolRuntime
{
public:
    explicit ManagedToolRuntime(ManagedToolRuntimeOptions options = {});

    [[nodiscard]] ManagedToolDescriptor Resolve(ManagedToolId id) const;
    [[nodiscard]] ManagedToolInvocation PrepareInvocation(
        ManagedToolId id, std::vector<std::string> arguments,
        ToolProcessRequest processOptions = {}) const;
    [[nodiscard]] ManagedToolProcessResult Run(
        ManagedToolId id, std::vector<std::string> arguments,
        ToolProcessRequest processOptions = {}) const;

private:
    ManagedToolRuntimeOptions options_;
};

[[nodiscard]] const char* ManagedToolName(ManagedToolId id) noexcept;

} // namespace SagaEditor
