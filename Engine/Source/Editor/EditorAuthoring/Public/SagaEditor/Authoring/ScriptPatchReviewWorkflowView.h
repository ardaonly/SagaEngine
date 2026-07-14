#pragma once

#include "SagaEditor/Authoring/ScriptPatchPreviewView.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

struct ScriptPatchReviewArtifactView
{
    std::string kind;
    std::filesystem::path path;
    bool exists = false;
    std::string status;
};

struct ScriptPatchReviewActionDescriptor
{
    std::string id;
    bool available = false;
    std::string disabledReason;
};

struct ScriptPatchReviewWorkflowView
{
    bool ok = false;
    bool applyAvailable = false;
    bool mutatesSource = false;
    std::string status;
    std::string operation;
    std::string operationId;
    std::string nodeId;
    std::filesystem::path sourceFile;
    std::string baseSourceHash;
    std::string proposedSourceHash;
    std::string newSourceHash;
    ScriptSourceSpanView span;
    std::string oldText;
    std::string newText;
    std::string rollbackStatus;
    std::string reviewDecision;
    bool reviewApprovedMeansApplied = false;
    std::vector<std::string> staleArtifacts;
    std::vector<ScriptPatchReviewArtifactView> artifacts;
    std::vector<ScriptPatchReviewActionDescriptor> actions;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] ScriptPatchReviewWorkflowView LoadScriptPatchReviewWorkflowView(
    const TechnicalPreviewProjectView& project);

} // namespace SagaEditor::Authoring
