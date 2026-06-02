#include "SagaEditor/Authoring/ScriptPatchPreviewView.h"

#include <nlohmann/json.hpp>

#include <fstream>

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

[[nodiscard]] ScriptSourceSpanView ParsePreviewSpan(const Json& preview)
{
    ScriptSourceSpanView span;
    span.startByte = IntField(preview, "startByte");
    span.endByte = IntField(preview, "endByte");
    return span;
}

[[nodiscard]] Json ReadJsonFile(const fs::path& path,
                                std::vector<AuthoringDiagnostic>& diagnostics)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.Missing",
            "patch_preview.json is missing",
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

void LoadDiagnostics(const Json& json, ScriptPatchPreviewView& view)
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
            StringField(diagnostic, "sourceFile"),
        });
    }
}

} // namespace

ScriptPatchPreviewView LoadScriptPatchPreviewView(
    const TechnicalPreviewProjectView& project)
{
    ScriptPatchPreviewView view;
    if (!project.ok)
    {
        view.diagnostics = project.diagnostics;
        return view;
    }

    const Json json = ReadJsonFile(
        ArtifactRoot(project) / "patch_preview.json",
        view.diagnostics);
    if (!json.is_object())
    {
        return view;
    }

    view.status = StringField(json, "status");
    view.operation = StringField(json, "operation");
    view.nodeId = StringField(json, "nodeId");
    view.sourceFile = StringField(json, "sourceFile");
    view.baseSourceHash = StringField(json, "baseSourceHash");
    view.mutatesSource = BoolField(json, "mutatesSource");
    view.applyAvailable = false;
    LoadDiagnostics(json, view);

    const auto previewIt = json.find("preview");
    if (previewIt != json.end() && previewIt->is_object())
    {
        view.span = ParsePreviewSpan(*previewIt);
        view.oldText = StringField(*previewIt, "oldText");
        view.newText = StringField(*previewIt, "newText");
    }

    view.ok = view.status == "Passed" && !view.mutatesSource;
    return view;
}

} // namespace SagaEditor::Authoring
