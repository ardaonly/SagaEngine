#pragma once

#include "SagaEditor/Authoring/TechnicalPreviewProjectView.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

enum class ScriptArtifactStatus
{
    Ready,
    Missing,
    MissingSource,
    Stale,
    Invalid,
    UnknownFreshness,
};

struct ScriptArtifactSourceStatus
{
    std::filesystem::path sourceFile;
    std::string expectedHash;
    std::string actualHash;
    ScriptArtifactStatus status = ScriptArtifactStatus::UnknownFreshness;
};

struct ScriptArtifactEntry
{
    std::string artifactName;
    std::filesystem::path path;
    bool required = false;
    ScriptArtifactStatus status = ScriptArtifactStatus::Missing;
    std::vector<ScriptArtifactSourceStatus> sources;
    std::vector<AuthoringDiagnostic> diagnostics;
};

struct ScriptArtifactIndex
{
    std::filesystem::path artifactRoot;
    ScriptArtifactStatus overallStatus = ScriptArtifactStatus::Missing;
    std::vector<ScriptArtifactEntry> artifacts;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] const char* ToString(ScriptArtifactStatus status) noexcept;

[[nodiscard]] ScriptArtifactIndex BuildScriptArtifactIndex(
    const TechnicalPreviewProjectView& project);

} // namespace SagaEditor::Authoring
