// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"
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

struct ProjectTemplateRequest
{
    std::filesystem::path destination;
    std::string displayName;
    std::filesystem::path seed;
    std::filesystem::path sagaPackagerExecutable;
    std::filesystem::path engineRoot;
    std::filesystem::path runtimeBridge;
};

/// Resolves the repository-owned Starter Arena seed and Campaign 1 tools.
/// Environment overrides are transport-only and never enter project truth.
[[nodiscard]] ProjectTemplateRequest MakeStarterArenaTemplateRequest(
    std::filesystem::path destination, std::string displayName);

class IProjectToolClient
{
public:
    virtual ~IProjectToolClient() = default;
    [[nodiscard]] virtual ProjectToolResult Validate(
        const std::filesystem::path& manifest) const = 0;
    [[nodiscard]] virtual ProjectToolResult CreateStarterArena(
        const ProjectTemplateRequest& request) const = 0;
};

class SagaProjectKitClient final : public IProjectToolClient
{
public:
    SagaProjectKitClient(std::filesystem::path executable = "sagaproject",
                         std::chrono::milliseconds timeout = std::chrono::seconds(30));

    [[nodiscard]] ProjectToolResult Validate(
        const std::filesystem::path& manifest) const override;
    [[nodiscard]] ProjectToolResult CreateStarterArena(
        const ProjectTemplateRequest& request) const override;

private:
    [[nodiscard]] ProjectToolResult RunReportCommand(
        std::vector<std::string> arguments,
        const std::filesystem::path& reportPath) const;

    std::filesystem::path executable_;
    std::chrono::milliseconds timeout_;
};

} // namespace SagaEditor
