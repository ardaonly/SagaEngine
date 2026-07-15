#include "SagaEditor/Authoring/ScriptPatchReviewWorkflowView.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <utility>

namespace SagaEditor::Authoring
{
namespace
{

using Json = nlohmann::json;
namespace fs = std::filesystem;

[[nodiscard]] fs::path ArtifactRoot(const ProjectReadinessView& project)
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

[[nodiscard]] bool BoolField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_boolean() && it->get<bool>();
}

[[nodiscard]] int IntField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer())
    {
        return 0;
    }
    return it->get<int>();
}

[[nodiscard]] ScriptSourceSpanView ParseSpan(const Json& object)
{
    return ScriptSourceSpanView{
        IntField(object, "startLine"),
        IntField(object, "startColumn"),
        IntField(object, "endLine"),
        IntField(object, "endColumn"),
        IntField(object, "startByte"),
        IntField(object, "endByte"),
    };
}

[[nodiscard]] Json ReadOptionalJsonFile(
    const fs::path& path,
    ScriptPatchReviewWorkflowView& view,
    const std::string& kind)
{
    if (!fs::exists(path))
    {
        view.artifacts.push_back(ScriptPatchReviewArtifactView{
            kind,
            path,
            false,
            "Missing",
        });
        return Json();
    }

    std::ifstream input(path);
    try
    {
        Json json;
        input >> json;
        view.artifacts.push_back(ScriptPatchReviewArtifactView{
            kind,
            path,
            true,
            StringField(json, "status"),
        });
        return json;
    }
    catch (const Json::exception& error)
    {
        view.artifacts.push_back(ScriptPatchReviewArtifactView{
            kind,
            path,
            true,
            "Invalid",
        });
        view.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.PatchReview.ArtifactInvalid",
            error.what(),
            path.string(),
        });
        return Json();
    }
}

void LoadDiagnostics(const Json& json,
                     const fs::path& path,
                     ScriptPatchReviewWorkflowView& view)
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
        view.diagnostics.push_back(AuthoringDiagnostic{
            StringField(diagnostic, "code"),
            StringField(diagnostic, "message"),
            StringField(diagnostic, "sourceFile").empty()
                ? path.string()
                : StringField(diagnostic, "sourceFile"),
        });
    }
}

void LoadStaleArtifacts(const Json& json, ScriptPatchReviewWorkflowView& view)
{
    const auto staleIt = json.find("staleArtifacts");
    if (staleIt == json.end() || !staleIt->is_array())
    {
        return;
    }
    for (const Json& stale : *staleIt)
    {
        if (stale.is_string())
        {
            view.staleArtifacts.push_back(stale.get<std::string>());
        }
    }
}

void MergePatchFields(const Json& json,
                      const fs::path& path,
                      ScriptPatchReviewWorkflowView& view)
{
    if (!json.is_object())
    {
        return;
    }

    const std::string status = StringField(json, "status");
    if (!status.empty())
    {
        view.status = status;
    }
    if (view.operation.empty())
    {
        view.operation = StringField(json, "operation");
    }
    if (view.operationId.empty())
    {
        view.operationId = StringField(json, "operationId");
    }
    if (view.nodeId.empty())
    {
        view.nodeId = StringField(json, "nodeId");
    }
    if (view.sourceFile.empty())
    {
        view.sourceFile = StringField(json, "sourceFile");
    }
    if (view.baseSourceHash.empty())
    {
        view.baseSourceHash = StringField(json, "baseSourceHash");
    }
    if (view.proposedSourceHash.empty())
    {
        view.proposedSourceHash = StringField(json, "proposedSourceHash");
    }
    if (view.newSourceHash.empty())
    {
        view.newSourceHash = StringField(json, "newSourceHash");
    }
    if (view.oldText.empty())
    {
        view.oldText = StringField(json, "oldText");
    }
    if (view.newText.empty())
    {
        view.newText = StringField(json, "newText");
    }
    if (view.rollbackStatus.empty())
    {
        view.rollbackStatus = StringField(json, "rollbackStatus");
    }
    view.mutatesSource = view.mutatesSource || BoolField(json, "mutatesSource");

    const auto spanIt = json.find("sourceSpan");
    if (spanIt != json.end() && spanIt->is_object())
    {
        view.span = ParseSpan(*spanIt);
    }
    const auto evaluationIt = json.find("evaluation");
    if (evaluationIt != json.end() && evaluationIt->is_object())
    {
        if (view.oldText.empty())
        {
            view.oldText = StringField(*evaluationIt, "oldText");
        }
        if (view.newText.empty())
        {
            view.newText = StringField(*evaluationIt, "newText");
        }
        view.span.startByte = IntField(*evaluationIt, "startByte");
        view.span.endByte = IntField(*evaluationIt, "endByte");
    }
    const auto byteDiffIt = json.find("byteDiff");
    if (byteDiffIt != json.end() && byteDiffIt->is_object())
    {
        view.span.startByte = IntField(*byteDiffIt, "startByte");
        view.span.endByte = IntField(*byteDiffIt, "endByte");
    }

    if (StringField(json, "decision") == "Approved")
    {
        view.reviewDecision = "Approved";
        view.reviewApprovedMeansApplied = false;
    }
    else if (!StringField(json, "decision").empty())
    {
        view.reviewDecision = StringField(json, "decision");
    }

    LoadDiagnostics(json, path, view);
    LoadStaleArtifacts(json, view);
}

void AddDisabledActions(ScriptPatchReviewWorkflowView& view)
{
    constexpr const char* kReason = "SagaScript-owned source mutation only.";
    view.applyAvailable = false;
    view.actions.push_back(ScriptPatchReviewActionDescriptor{
        "apply",
        false,
        kReason,
    });
    view.actions.push_back(ScriptPatchReviewActionDescriptor{
        "rollback",
        false,
        kReason,
    });
    view.actions.push_back(ScriptPatchReviewActionDescriptor{
        "cancel",
        false,
        "Report-only editor workflow state.",
    });
}

} // namespace

ScriptPatchReviewWorkflowView LoadScriptPatchReviewWorkflowView(
    const ProjectReadinessView& project)
{
    ScriptPatchReviewWorkflowView view;
    AddDisabledActions(view);

    if (!project.ok)
    {
        view.diagnostics = project.diagnostics;
        return view;
    }

    const fs::path root = ArtifactRoot(project);
    const std::pair<const char*, const char*> reports[] = {
        {"evaluation", "patch_evaluation.json"},
        {"apply", "patch_apply_report.json"},
        {"diff", "patch_diff_report.json"},
        {"review", "patch_review_report.json"},
        {"rollback", "patch_rollback_report.json"},
    };

    bool sawExistingReport = false;
    for (const auto& [kind, fileName] : reports)
    {
        const fs::path path = root / fileName;
        const Json json = ReadOptionalJsonFile(path, view, kind);
        if (json.is_object())
        {
            sawExistingReport = true;
            MergePatchFields(json, path, view);
        }
    }

    if (!sawExistingReport)
    {
        view.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.PatchReview.ReportMissing",
            "no SagaScript patch review artifacts are present",
            root.string(),
        });
    }

    view.applyAvailable = false;
    view.ok = sawExistingReport && view.diagnostics.empty();
    return view;
}

} // namespace SagaEditor::Authoring
