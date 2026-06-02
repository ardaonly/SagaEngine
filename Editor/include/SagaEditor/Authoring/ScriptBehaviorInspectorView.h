#pragma once

#include "SagaEditor/Authoring/ScriptBehaviorProjectionView.h"

#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

struct ScriptBehaviorInspectorView
{
    std::string behaviorId;
    std::string apiLevel;
    std::string apiDomain;
    std::string compatibility;
    std::string projectionStatus;
    std::string runtimeBindingStatus;
    std::string nodeMetadataStatus;
    bool hasRuntimeBinding = false;
    std::string bindingStatus;
    ScriptSourceLinkView sourceLink;
    std::vector<ScriptBlockProjectionNodeView> nodes;
    std::vector<std::string> runtimeProofStates;
    std::vector<std::string> compatibilityClassifications;
    std::vector<std::string> unsupportedReasons;
    std::vector<AuthoringDiagnostic> diagnostics;
};

struct ScriptBehaviorInspectorLoadResult
{
    bool ok = false;
    std::vector<ScriptBehaviorInspectorView> behaviors;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] ScriptBehaviorInspectorLoadResult LoadScriptBehaviorInspectorViews(
    const TechnicalPreviewProjectView& project);

} // namespace SagaEditor::Authoring
