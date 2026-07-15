#pragma once

#include "SagaEditor/Authoring/ScriptArtifactIndex.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

struct ScriptSourceSpanView
{
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    int startByte = 0;
    int endByte = 0;
};

struct ScriptSourceLinkView
{
    std::string behaviorId;
    std::string nodeId;
    std::filesystem::path sourceFile;
    std::string sourceHash;
    ScriptArtifactStatus freshness = ScriptArtifactStatus::UnknownFreshness;
    ScriptSourceSpanView span;
};

struct ScriptBlockProjectionNodeView
{
    std::string nodeId;
    std::string kind;
    std::string displayName;
    std::string apiLevel;
    std::string apiDomain;
    std::string capability;
    std::string projectionCompatibility;
    std::string compatibilityClassification;
    std::string runtimeProofState;
    std::string nodeMetadataStatus;
    bool readOnly = true;
    std::string opaqueReason;
    std::string literalValue;
    ScriptSourceLinkView sourceLink;
};

struct ScriptBehaviorProjectionView
{
    std::string behaviorId;
    std::string apiLevel;
    std::string apiDomain;
    std::string compatibility;
    ScriptSourceLinkView sourceLink;
    std::vector<ScriptBlockProjectionNodeView> nodes;
};

struct ScriptBehaviorProjectionLoadResult
{
    bool ok = false;
    std::vector<ScriptBehaviorProjectionView> behaviors;
    std::vector<ScriptSourceLinkView> sourceLinks;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] ScriptBehaviorProjectionLoadResult LoadScriptBehaviorProjectionViews(
    const ProjectReadinessView& project);

[[nodiscard]] ScriptBehaviorProjectionLoadResult LoadScriptSourceLinkViews(
    const ProjectReadinessView& project);

} // namespace SagaEditor::Authoring
