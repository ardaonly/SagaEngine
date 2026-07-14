/// @file EditorProjectInspection.h
/// @brief Defines the Editor-owned project inspection report service.

#pragma once

#include <filesystem>
#include <string>

namespace SagaEditor
{

/// Editor-owned request for generating a bounded read-only project report.
struct EditorProjectInspectionRequest
{
    std::filesystem::path projectPath;
    std::filesystem::path reportPath;
    std::string requestedProfileId;
};

/// Result from writing an Editor project inspection report.
struct EditorProjectInspectionResult
{
    bool ok = false;
    std::filesystem::path reportPath;
    std::string status;
    std::string error;
};

/// Build and write the current schema-2 Editor inspection contract.
[[nodiscard]] EditorProjectInspectionResult InspectEditorProject(
    const EditorProjectInspectionRequest& request);

} // namespace SagaEditor
