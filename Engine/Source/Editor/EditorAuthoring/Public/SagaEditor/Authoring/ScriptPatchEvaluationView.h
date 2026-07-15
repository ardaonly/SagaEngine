#pragma once

#include "SagaEditor/Authoring/ScriptBehaviorProjectionView.h"

#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

struct ScriptPatchEvaluationView
{
    bool ok = false;
    bool mutatesSource = false;
    bool applyAvailable = false;
    std::string status;
    std::string operation;
    std::string nodeId;
    std::filesystem::path sourceFile;
    std::string baseSourceHash;
    ScriptSourceSpanView span;
    std::string oldText;
    std::string newText;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] ScriptPatchEvaluationView LoadScriptPatchEvaluationView(
    const ProjectReadinessView& project);

} // namespace SagaEditor::Authoring
