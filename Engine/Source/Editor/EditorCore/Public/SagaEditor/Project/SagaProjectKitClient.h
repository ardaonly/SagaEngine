// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Processes/ManagedToolRuntime.h"
#include "SagaEditor/Processes/ToolProcessRunner.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEditor
{

struct ValidatedProjectDescriptor
{
    struct SourceReference
    {
        std::string category;
        std::string id;
        std::string kind;
        std::filesystem::path relativePath;
        std::filesystem::path absolutePath;
    };

    std::string projectId;
    std::string displayName;
    std::filesystem::path projectRoot;
    std::filesystem::path manifestPath;
    std::string engineTargetVersion;
    std::uint32_t schemaVersion = 0;
    std::vector<SourceReference> sources;
};

struct ProjectToolResult
{
    bool passed = false;
    ValidatedProjectDescriptor project;
    std::vector<DocumentDiagnostic> diagnostics;
    ToolProcessResult process;
};

enum class ProjectTemplateId
{
    StarterArena,
    ArenaRush,
};

struct ProjectTemplateRequest
{
    ProjectTemplateId templateId = ProjectTemplateId::StarterArena;
    std::filesystem::path destination;
    std::string displayName;
    std::filesystem::path seed;
    std::filesystem::path engineRoot;
    std::filesystem::path runtimeBridge;
    ManagedToolRuntimeOptions managedRuntime;
};

struct EditorToolchainPaths
{
    std::filesystem::path productRoot;
    std::filesystem::path engineRoot;
    std::filesystem::path starterArenaSeed;
    std::filesystem::path arenaRushSeed;
    std::filesystem::path runtimeBridge;
    ManagedToolRuntimeOptions managedRuntime;
};

/// Locates the installed/repository toolchain without introducing project or
/// sample identity into callers. Environment overrides are local transport.
[[nodiscard]] EditorToolchainPaths ResolveEditorToolchain(
    const std::filesystem::path& productExecutable = {});

/// Resolves the repository-owned Starter Arena seed and project tools.
/// Environment overrides are transport-only and never enter project truth.
[[nodiscard]] ProjectTemplateRequest MakeStarterArenaTemplateRequest(
    std::filesystem::path destination, std::string displayName,
    const std::filesystem::path& productExecutable = {});

[[nodiscard]] ProjectTemplateRequest MakeProjectTemplateRequest(
    ProjectTemplateId templateId, std::filesystem::path destination,
    std::string displayName,
    const std::filesystem::path& productExecutable = {});

class IProjectToolClient
{
public:
    virtual ~IProjectToolClient() = default;
    [[nodiscard]] virtual ProjectToolResult Validate(
        const std::filesystem::path& manifest) const = 0;
    /// Creates transactionally and returns the final validated project
    /// descriptor. A passing result is safe for the workspace to materialize.
    [[nodiscard]] virtual ProjectToolResult CreateStarterArena(
        const ProjectTemplateRequest& request) const = 0;
    [[nodiscard]] virtual ProjectToolResult CreateProject(
        const ProjectTemplateRequest& request) const
    {
        return CreateStarterArena(request);
    }
};

class SagaProjectKitClient final : public IProjectToolClient
{
public:
    SagaProjectKitClient(ManagedToolRuntimeOptions runtime = {},
                         std::chrono::milliseconds timeout = std::chrono::seconds(30));

    [[nodiscard]] ProjectToolResult Validate(
        const std::filesystem::path& manifest) const override;
    [[nodiscard]] ProjectToolResult CreateStarterArena(
        const ProjectTemplateRequest& request) const override;
    [[nodiscard]] ProjectToolResult CreateProject(
        const ProjectTemplateRequest& request) const override;

private:
    [[nodiscard]] ProjectToolResult RunReportCommand(
        std::vector<std::string> arguments,
        const std::filesystem::path& reportPath) const;

    ManagedToolRuntime runtime_;
    std::chrono::milliseconds timeout_;
};

} // namespace SagaEditor
