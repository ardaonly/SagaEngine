#include "SagaEditor/Authoring/ScriptBehaviorInspectorView.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace SagaEditor::Authoring
{
namespace
{

using Json = nlohmann::json;
namespace fs = std::filesystem;

[[nodiscard]] fs::path ArtifactRoot(const TechnicalPreviewProjectView& project)
{
    return (project.projectRoot / "Build" / "SagaScript").lexically_normal();
}

[[nodiscard]] std::string StringField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return {};
    }
    return it->get<std::string>();
}

[[nodiscard]] Json ReadJsonFile(const fs::path& path,
                                std::vector<AuthoringDiagnostic>& diagnostics)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.Missing",
            "script artifact is missing",
            path.string(),
        });
        return Json();
    }
    try
    {
        Json json;
        input >> json;
        return json;
    }
    catch (const Json::exception& error)
    {
        diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.Invalid",
            error.what(),
            path.string(),
        });
        return Json();
    }
}

void LoadDiagnostics(const Json& json,
                     const fs::path& path,
                     std::vector<AuthoringDiagnostic>& diagnostics)
{
    const auto diagnosticsIt = json.find("diagnostics");
    if (diagnosticsIt == json.end() || !diagnosticsIt->is_array())
    {
        return;
    }

    for (const Json& diagnostic : *diagnosticsIt)
    {
        if (!diagnostic.is_object())
        {
            continue;
        }
        diagnostics.push_back(AuthoringDiagnostic{
            StringField(diagnostic, "code"),
            StringField(diagnostic, "message"),
            StringField(diagnostic, "sourceFile").empty()
                ? path.string()
                : StringField(diagnostic, "sourceFile"),
        });
    }
}

struct RuntimeNodeEvidence
{
    std::string compatibilityClassification;
    std::string runtimeProof;
};

struct RuntimeBindingEvidence
{
    std::unordered_set<std::string> behaviorIds;
    std::unordered_map<std::string, RuntimeNodeEvidence> nodesByKey;
};

[[nodiscard]] RuntimeBindingEvidence LoadRuntimeBindingEvidence(
    const TechnicalPreviewProjectView& project,
    std::vector<AuthoringDiagnostic>& diagnostics)
{
    RuntimeBindingEvidence evidence;
    const Json bindings = ReadJsonFile(
        ArtifactRoot(project) / "runtime_bindings.json",
        diagnostics);
    if (!bindings.is_object())
    {
        return evidence;
    }

    const auto bindingsIt = bindings.find("bindings");
    if (bindingsIt == bindings.end() || !bindingsIt->is_array())
    {
        return evidence;
    }

    for (const Json& binding : *bindingsIt)
    {
        if (binding.is_object())
        {
            const std::string behaviorId = StringField(binding, "behaviorId");
            evidence.behaviorIds.insert(behaviorId);
            const auto nodesIt = binding.find("nodes");
            if (nodesIt == binding.end() || !nodesIt->is_array())
            {
                continue;
            }
            for (const Json& node : *nodesIt)
            {
                if (!node.is_object())
                {
                    continue;
                }
                RuntimeNodeEvidence nodeEvidence;
                nodeEvidence.compatibilityClassification =
                    StringField(node, "compatibilityClassification");
                nodeEvidence.runtimeProof = StringField(node, "runtimeProof");
                evidence.nodesByKey[behaviorId + "\n" +
                    StringField(node, "nodeId")] = nodeEvidence;
            }
        }
    }
    LoadDiagnostics(bindings, ArtifactRoot(project) / "runtime_bindings.json",
                    diagnostics);
    return evidence;
}

struct NodeMetadataEvidence
{
    std::unordered_map<std::string, ScriptBlockProjectionNodeView> nodesById;
};

[[nodiscard]] NodeMetadataEvidence LoadNodeMetadataEvidence(
    const TechnicalPreviewProjectView& project,
    std::vector<AuthoringDiagnostic>& diagnostics)
{
    NodeMetadataEvidence evidence;
    const fs::path path = ArtifactRoot(project) / "node_metadata.json";
    const Json metadata = ReadJsonFile(path, diagnostics);
    if (!metadata.is_object())
    {
        return evidence;
    }

    const auto nodesIt = metadata.find("nodes");
    if (nodesIt != metadata.end() && nodesIt->is_array())
    {
        for (const Json& node : *nodesIt)
        {
            if (!node.is_object())
            {
                continue;
            }
            ScriptBlockProjectionNodeView nodeView;
            nodeView.nodeId = StringField(node, "nodeId");
            nodeView.displayName = StringField(node, "displayName");
            nodeView.apiDomain = StringField(node, "apiDomain");
            nodeView.apiLevel = StringField(node, "apiLevel");
            nodeView.capability = StringField(node, "capability");
            nodeView.projectionCompatibility =
                StringField(node, "projectionCompatibility");
            evidence.nodesById[nodeView.nodeId] = nodeView;
        }
    }
    LoadDiagnostics(metadata, path, diagnostics);
    return evidence;
}

[[nodiscard]] std::unordered_map<std::string, std::string>
LoadCompatibilityClassifications(
    const TechnicalPreviewProjectView& project,
    std::vector<AuthoringDiagnostic>& diagnostics)
{
    std::unordered_map<std::string, std::string> classifications;
    const fs::path path =
        ArtifactRoot(project) / "csharp_compatibility_profile_v2.json";
    const Json profile = ReadJsonFile(path, diagnostics);
    if (!profile.is_object())
    {
        return classifications;
    }

    const auto constructsIt = profile.find("constructs");
    if (constructsIt != profile.end() && constructsIt->is_array())
    {
        for (const Json& construct : *constructsIt)
        {
            if (!construct.is_object())
            {
                continue;
            }
            const std::string id = StringField(construct, "constructId");
            if (!id.empty())
            {
                classifications[id] = StringField(construct, "classification");
            }
        }
    }
    LoadDiagnostics(profile, path, diagnostics);
    return classifications;
}

void LoadOptionalValidationDiagnostics(
    const TechnicalPreviewProjectView& project,
    std::vector<AuthoringDiagnostic>& diagnostics)
{
    const fs::path path =
        ArtifactRoot(project) / "script_artifact_validation_report.json";
    if (!fs::exists(path))
    {
        return;
    }
    const Json validation = ReadJsonFile(path, diagnostics);
    if (validation.is_object())
    {
        LoadDiagnostics(validation, path, diagnostics);
    }
}

[[nodiscard]] bool IsRuntimeProof(const std::string& runtimeProof)
{
    return runtimeProof == "RuntimeBackedWithEvidence";
}

} // namespace

ScriptBehaviorInspectorLoadResult LoadScriptBehaviorInspectorViews(
    const TechnicalPreviewProjectView& project)
{
    ScriptBehaviorInspectorLoadResult result;
    if (!project.ok)
    {
        result.diagnostics = project.diagnostics;
        return result;
    }

    ScriptBehaviorProjectionLoadResult projection =
        LoadScriptBehaviorProjectionViews(project);
    result.diagnostics = projection.diagnostics;

    std::vector<AuthoringDiagnostic> bindingDiagnostics;
    const RuntimeBindingEvidence runtimeEvidence =
        LoadRuntimeBindingEvidence(project, bindingDiagnostics);
    result.diagnostics.insert(
        result.diagnostics.end(),
        bindingDiagnostics.begin(),
        bindingDiagnostics.end());

    std::vector<AuthoringDiagnostic> metadataDiagnostics;
    const NodeMetadataEvidence metadataEvidence =
        LoadNodeMetadataEvidence(project, metadataDiagnostics);
    result.diagnostics.insert(
        result.diagnostics.end(),
        metadataDiagnostics.begin(),
        metadataDiagnostics.end());

    std::vector<AuthoringDiagnostic> compatibilityDiagnostics;
    const std::unordered_map<std::string, std::string> compatibility =
        LoadCompatibilityClassifications(project, compatibilityDiagnostics);
    result.diagnostics.insert(
        result.diagnostics.end(),
        compatibilityDiagnostics.begin(),
        compatibilityDiagnostics.end());

    LoadOptionalValidationDiagnostics(project, result.diagnostics);

    for (const ScriptBehaviorProjectionView& behavior : projection.behaviors)
    {
        ScriptBehaviorInspectorView view;
        view.behaviorId = behavior.behaviorId;
        view.apiLevel = behavior.apiLevel;
        view.apiDomain = behavior.apiDomain;
        view.compatibility = behavior.compatibility;
        view.projectionStatus = projection.ok ? "Ready" : "Diagnostics";
        view.sourceLink = behavior.sourceLink;
        view.nodes = behavior.nodes;
        view.hasRuntimeBinding =
            runtimeEvidence.behaviorIds.contains(behavior.behaviorId);
        view.bindingStatus = view.hasRuntimeBinding ? "Ready" : "Missing";
        view.runtimeBindingStatus = view.bindingStatus;
        view.nodeMetadataStatus =
            metadataEvidence.nodesById.empty() ? "Missing" : "Ready";

        for (ScriptBlockProjectionNodeView& node : view.nodes)
        {
            const auto metadataIt = metadataEvidence.nodesById.find(node.nodeId);
            if (metadataIt != metadataEvidence.nodesById.end())
            {
                const ScriptBlockProjectionNodeView& metadataNode =
                    metadataIt->second;
                if (node.capability.empty())
                {
                    node.capability = metadataNode.capability;
                }
                if (node.projectionCompatibility.empty())
                {
                    node.projectionCompatibility =
                        metadataNode.projectionCompatibility;
                }
                node.nodeMetadataStatus = "Ready";
            }
            else
            {
                node.nodeMetadataStatus = "Missing";
            }

            const auto runtimeIt = runtimeEvidence.nodesByKey.find(
                behavior.behaviorId + "\n" + node.nodeId);
            if (runtimeIt != runtimeEvidence.nodesByKey.end())
            {
                node.compatibilityClassification =
                    runtimeIt->second.compatibilityClassification;
                node.runtimeProofState = runtimeIt->second.runtimeProof;
            }
            if (node.compatibilityClassification.empty())
            {
                const auto compatibilityIt = compatibility.find(node.nodeId);
                if (compatibilityIt != compatibility.end())
                {
                    node.compatibilityClassification =
                        compatibilityIt->second;
                }
            }
            if (node.runtimeProofState.empty())
            {
                if (node.capability == "ProjectionOnly" ||
                    node.capability == "Deferred")
                {
                    node.runtimeProofState = "NotRuntimeProof";
                }
                else if (node.capability == "RuntimeBacked")
                {
                    node.runtimeProofState =
                        "RuntimeBackedWithEvidenceMissing";
                }
                else
                {
                    node.runtimeProofState = "Unknown";
                }
            }

            view.runtimeProofStates.push_back(node.runtimeProofState);
            view.compatibilityClassifications.push_back(
                node.compatibilityClassification);

            if (!node.opaqueReason.empty())
            {
                view.unsupportedReasons.push_back(node.opaqueReason);
            }
            if (!IsRuntimeProof(node.runtimeProofState))
            {
                view.diagnostics.push_back(AuthoringDiagnostic{
                    "Editor.BehaviorInspector.NodeNotRuntimeProof",
                    "node is visible in editor evidence but is not runtime proof",
                    node.sourceLink.sourceFile.string(),
                });
            }
        }
        result.diagnostics.insert(
            result.diagnostics.end(),
            view.diagnostics.begin(),
            view.diagnostics.end());
        result.behaviors.push_back(view);
    }

    result.ok = projection.ok &&
        bindingDiagnostics.empty() &&
        metadataDiagnostics.empty() &&
        compatibilityDiagnostics.empty();
    return result;
}

} // namespace SagaEditor::Authoring
