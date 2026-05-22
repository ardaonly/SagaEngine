/// @file CompositionCompiler.cpp
/// @brief SDE-adjacent saga.editor composition artifact writer implementation.

#include "SagaEditorComposition/CompositionCompiler.h"

#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/Compiler/CompilerFacade.h"
#include "SDE/Core/StableHash.h"
#include "SDE/Core/Version.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <variant>

namespace SagaEditorComposition
{
namespace
{

using Json = nlohmann::json;

constexpr const char* kArtifactFile = "saga.editor.composition.json";
constexpr const char* kManifestFile = "saga.editor.composition.manifest.json";
constexpr const char* kDiagnosticsFile = "saga.editor.composition.diagnostics.json";
constexpr const char* kSourceMapFile = "saga.editor.composition.sourcemap.json";
constexpr const char* kDependencyFile = "saga.editor.composition.dependencies.json";

constexpr const char* kArtifactKind = "saga.editor.composition";
constexpr const char* kManifestKind = "saga.editor.composition.manifest";
constexpr const char* kSchemaPackage = "saga.editor";
constexpr uint32_t kArtifactVersion = 1;
constexpr uint32_t kManifestVersion = 1;
constexpr uint32_t kSchemaPackageVersion = 1;

[[nodiscard]] CompositionOutputPaths MakeOutputPaths(
    const std::filesystem::path& outputRoot)
{
    return {
        outputRoot / kArtifactFile,
        outputRoot / kManifestFile,
        outputRoot / kDiagnosticsFile,
        outputRoot / kSourceMapFile,
        outputRoot / kDependencyFile,
    };
}

[[nodiscard]] bool HasAnyExplicitOutput(
    const CompositionOutputPaths& outputs)
{
    return !outputs.artifactPath.empty() ||
           !outputs.manifestPath.empty() ||
           !outputs.diagnosticsPath.empty() ||
           !outputs.sourceMapPath.empty() ||
           !outputs.dependencyManifestPath.empty();
}

[[nodiscard]] bool HasCompleteExplicitOutputs(
    const CompositionOutputPaths& outputs)
{
    return !outputs.artifactPath.empty() &&
           !outputs.manifestPath.empty() &&
           !outputs.diagnosticsPath.empty() &&
           !outputs.sourceMapPath.empty() &&
           !outputs.dependencyManifestPath.empty();
}

[[nodiscard]] std::filesystem::path ResolveOutputRoot(
    const CompositionCompileRequest& request)
{
    if (!request.outputRoot.empty())
    {
        return request.outputRoot;
    }
    return request.workspaceRoot / "artifacts" / "saga.editor";
}

[[nodiscard]] CompositionOutputPaths ResolveOutputPaths(
    const CompositionCompileRequest& request)
{
    if (HasCompleteExplicitOutputs(request.explicitOutputs))
    {
        return request.explicitOutputs;
    }
    if (!request.buildRoot.empty())
    {
        return MakeBuildOutputPaths(request.buildRoot);
    }
    return MakeOutputPaths(ResolveOutputRoot(request));
}

[[nodiscard]] std::string ManifestPathReference(
    const std::filesystem::path& path,
    const std::filesystem::path& manifestPath)
{
    const std::filesystem::path manifestDirectory = manifestPath.parent_path();
    if (manifestDirectory.empty())
    {
        return path.generic_string();
    }

    std::error_code ec;
    const std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
    if (ec)
    {
        return path.generic_string();
    }

    const std::filesystem::path absoluteBase = std::filesystem::absolute(manifestDirectory, ec);
    if (ec)
    {
        return path.generic_string();
    }

    const std::filesystem::path relative = absolutePath.lexically_relative(absoluteBase);
    if (relative.empty())
    {
        return path.generic_string();
    }
    return relative.generic_string();
}

[[nodiscard]] SDE::Diagnostic MakeDiagnostic(SDE::Severity severity,
                                             SDE::DiagnosticCategory category,
                                             SDE::SourceLocation location,
                                             std::string code,
                                             std::string message)
{
    SDE::Diagnostic diagnostic = severity == SDE::Severity::Fatal
        ? SDE::Diagnostic::MakeFatal(std::move(location), std::move(code), std::move(message))
        : SDE::Diagnostic::MakeError(std::move(location), std::move(code), std::move(message));
    diagnostic.severity = severity;
    diagnostic.category = category;
    return diagnostic;
}

[[nodiscard]] bool HasErrors(const std::vector<SDE::Diagnostic>& diagnostics)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const SDE::Diagnostic& diagnostic)
    {
        return diagnostic.severity == SDE::Severity::Error ||
               diagnostic.severity == SDE::Severity::Fatal;
    });
}

[[nodiscard]] bool HasWarnings(const std::vector<SDE::Diagnostic>& diagnostics)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const SDE::Diagnostic& diagnostic)
    {
        return diagnostic.severity == SDE::Severity::Warning;
    });
}

[[nodiscard]] SDE::CompileState StateFromDiagnostics(
    SDE::CompileState base,
    const std::vector<SDE::Diagnostic>& diagnostics) noexcept
{
    if (HasErrors(diagnostics))
    {
        return SDE::Merge(base, SDE::CompileState::ValidationFailed);
    }
    if (HasWarnings(diagnostics))
    {
        return SDE::Merge(base, SDE::CompileState::WithWarnings);
    }
    return base;
}

[[nodiscard]] const SDE::CompiledValue* Field(
    const SDE::CompiledInstance& instance,
    const std::string& fieldId)
{
    return instance.GetField(fieldId);
}

[[nodiscard]] std::string TextField(const SDE::CompiledInstance& instance,
                                    const std::string& fieldId,
                                    std::string fallback = {})
{
    const SDE::CompiledValue* value = Field(instance, fieldId);
    if (value == nullptr)
    {
        return fallback;
    }
    if (const auto* text = std::get_if<SDE::CompiledText>(&value->data))
    {
        return *text;
    }
    return fallback;
}

[[nodiscard]] bool BoolField(const SDE::CompiledInstance& instance,
                             const std::string& fieldId,
                             bool fallback)
{
    const SDE::CompiledValue* value = Field(instance, fieldId);
    if (value == nullptr)
    {
        return fallback;
    }
    if (const auto* boolean = std::get_if<SDE::CompiledBool>(&value->data))
    {
        return *boolean;
    }
    return fallback;
}

[[nodiscard]] std::string RefInstanceId(const SDE::CompiledInstance& instance,
                                        const std::string& fieldId)
{
    const SDE::CompiledValue* value = Field(instance, fieldId);
    if (value == nullptr)
    {
        return {};
    }
    if (const auto* ref = std::get_if<SDE::CompiledInstanceRef>(&value->data))
    {
        return ref->instanceId;
    }
    return {};
}

[[nodiscard]] std::vector<std::string> TextListField(
    const SDE::CompiledModelGraph& graph,
    const SDE::CompiledInstance& instance,
    const std::string& fieldId)
{
    std::vector<std::string> values;
    const SDE::CompiledValue* value = Field(instance, fieldId);
    if (value == nullptr)
    {
        return values;
    }

    const auto* array = std::get_if<SDE::CompiledArrayRef>(&value->data);
    if (array == nullptr)
    {
        return values;
    }

    for (uint32_t index = 0; index < array->range.count; ++index)
    {
        const SDE::CompiledValue& element =
            graph.Slab().At(array->range.offset + index);
        if (const auto* text = std::get_if<SDE::CompiledText>(&element.data))
        {
            values.push_back(*text);
        }
    }
    return values;
}

[[nodiscard]] Json TextListJson(const std::vector<std::string>& values)
{
    Json array = Json::array();
    for (const std::string& value : values)
    {
        array.push_back(value);
    }
    return array;
}

void AddSourceMapEntry(Json& sourceMap,
                       std::string artifactPointer,
                       const SDE::CompiledInstance& instance)
{
    sourceMap.push_back({
        {"artifactPointer", std::move(artifactPointer)},
        {"modelId", instance.modelId},
        {"instanceId", instance.instanceId},
        {"sourceFile", instance.origin.file},
        {"line", instance.origin.line},
        {"column", instance.origin.column},
    });
}

void AddWriterDiagnostic(std::vector<SDE::Diagnostic>& diagnostics,
                         SDE::DiagnosticCategory category,
                         const SDE::CompiledInstance* instance,
                         std::string code,
                         std::string message,
                         std::string referencedId = {})
{
    SDE::SourceLocation location;
    if (instance != nullptr)
    {
        location = instance->origin;
    }
    SDE::Diagnostic diagnostic = MakeDiagnostic(
        SDE::Severity::Error, category, std::move(location), std::move(code),
        std::move(message));
    if (instance != nullptr)
    {
        diagnostic.metadata["modelId"] = instance->modelId;
        diagnostic.metadata["instanceId"] = instance->instanceId;
    }
    if (!referencedId.empty())
    {
        diagnostic.metadata["referencedId"] = std::move(referencedId);
    }
    diagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] std::vector<const SDE::CompiledInstance*> InstancesForComposition(
    const SDE::CompiledModelGraph& graph,
    const std::string& modelId,
    const std::string& compositionId)
{
    std::vector<const SDE::CompiledInstance*> instances;
    for (const SDE::CompiledInstance* instance : graph.AllOf(modelId))
    {
        if (RefInstanceId(*instance, "composition") == compositionId)
        {
            instances.push_back(instance);
        }
    }
    return instances;
}

[[nodiscard]] const SDE::CompiledInstance* SelectComposition(
    const SDE::CompiledModelGraph& graph,
    const std::string& requestedId,
    std::vector<SDE::Diagnostic>& diagnostics)
{
    const std::vector<const SDE::CompiledInstance*> roots =
        graph.AllOf("EditorComposition");
    if (!requestedId.empty())
    {
        for (const SDE::CompiledInstance* root : roots)
        {
            if (root->instanceId == requestedId)
            {
                return root;
            }
        }
        AddWriterDiagnostic(diagnostics,
                            SDE::DiagnosticCategory::Schema,
                            nullptr,
                            "SDE_EDITOR_COMPOSITION_MISSING_ROOT",
                            "Requested EditorComposition root was not found.",
                            requestedId);
        return nullptr;
    }

    if (roots.empty())
    {
        AddWriterDiagnostic(diagnostics,
                            SDE::DiagnosticCategory::Schema,
                            nullptr,
                            "SDE_EDITOR_COMPOSITION_MISSING_ROOT",
                            "No EditorComposition root exists in the compiled graph.");
        return nullptr;
    }
    if (roots.size() > 1)
    {
        AddWriterDiagnostic(diagnostics,
                            SDE::DiagnosticCategory::Schema,
                            nullptr,
                            "SDE_EDITOR_COMPOSITION_DUPLICATE_ROOT",
                            "Multiple EditorComposition roots exist; pass --composition-id.");
        return nullptr;
    }
    return roots.front();
}

void ValidateRequiredText(std::vector<SDE::Diagnostic>& diagnostics,
                          const SDE::CompiledInstance& instance,
                          const std::string& fieldId,
                          const std::string& value)
{
    if (!value.empty())
    {
        return;
    }
    AddWriterDiagnostic(diagnostics,
                        SDE::DiagnosticCategory::Schema,
                        &instance,
                        "SDE_EDITOR_COMPOSITION_EMPTY_REQUIRED_FIELD",
                        "Required editor composition text field is empty: " + fieldId + ".");
}

void ValidateDuplicateIds(std::vector<SDE::Diagnostic>& diagnostics,
                          const Json& array,
                          const std::string& collectionName)
{
    std::set<std::string> ids;
    for (const Json& item : array)
    {
        const std::string id = item.value("id", std::string{});
        if (!id.empty() && !ids.insert(id).second)
        {
            SDE::Diagnostic diagnostic = MakeDiagnostic(
                SDE::Severity::Error,
                SDE::DiagnosticCategory::Schema,
                {},
                "SDE_EDITOR_COMPOSITION_DUPLICATE_ID",
                "Duplicate id in editor composition collection: " + collectionName + ".");
            diagnostic.metadata["id"] = id;
            diagnostic.metadata["collection"] = collectionName;
            diagnostics.push_back(std::move(diagnostic));
        }
    }
}

[[nodiscard]] std::set<std::string> IdSet(const Json& array)
{
    std::set<std::string> ids;
    for (const Json& item : array)
    {
        const std::string id = item.value("id", std::string{});
        if (!id.empty())
        {
            ids.insert(id);
        }
    }
    return ids;
}

void ValidateArtifactReferences(const Json& artifact,
                                const std::map<std::string, const SDE::CompiledInstance*>& origins,
                                std::vector<SDE::Diagnostic>& diagnostics)
{
    const std::set<std::string> actionIds = IdSet(artifact["actions"]);
    const std::set<std::string> panelIds = IdSet(artifact["panels"]);
    const std::set<std::string> menuIds = IdSet(artifact["menus"]);
    const std::set<std::string> layoutIds = IdSet(artifact["workspaceLayouts"]);
    const std::set<std::string> workspaceIds = IdSet(artifact["workspaces"]);

    auto originFor = [&](const std::string& id) -> const SDE::CompiledInstance*
    {
        const auto it = origins.find(id);
        return it == origins.end() ? nullptr : it->second;
    };

    for (const Json& action : artifact["actions"])
    {
        if (action.value("internalOnly", false) &&
            action.value("safeInProduct", true))
        {
            AddWriterDiagnostic(diagnostics,
                                SDE::DiagnosticCategory::Schema,
                                originFor(action.value("id", std::string{})),
                                "SDE_EDITOR_COMPOSITION_UNSAFE_PRODUCT_ACTION",
                                "Internal-only editor action cannot be marked safeInProduct.");
        }
    }

    for (const Json& panel : artifact["panels"])
    {
        if (panel.value("internalOnly", false) &&
            panel.value("defaultVisible", true))
        {
            AddWriterDiagnostic(diagnostics,
                                SDE::DiagnosticCategory::Schema,
                                originFor(panel.value("id", std::string{})),
                                "SDE_EDITOR_COMPOSITION_INTERNAL_PANEL_VISIBLE",
                                "Internal-only editor panel cannot be visible by default.");
        }
    }

    for (const Json& toolbar : artifact["toolbars"])
    {
        const SDE::CompiledInstance* origin =
            originFor(toolbar.value("id", std::string{}));
        for (const Json& actionId : toolbar["actions"])
        {
            const std::string id = actionId.get<std::string>();
            if (actionIds.find(id) == actionIds.end())
            {
                AddWriterDiagnostic(diagnostics,
                                    SDE::DiagnosticCategory::Reference,
                                    origin,
                                    "SDE_EDITOR_COMPOSITION_UNKNOWN_ACTION",
                                    "Toolbar references an unknown editor action.",
                                    id);
            }
        }
    }

    for (const Json& shortcut : artifact["shortcuts"])
    {
        const std::string actionId = shortcut.value("actionId", std::string{});
        if (actionIds.find(actionId) == actionIds.end())
        {
            AddWriterDiagnostic(diagnostics,
                                SDE::DiagnosticCategory::Reference,
                                originFor(shortcut.value("id", std::string{})),
                                "SDE_EDITOR_COMPOSITION_UNKNOWN_ACTION",
                                "Shortcut references an unknown editor action.",
                                actionId);
        }
    }

    for (const Json& menu : artifact["menus"])
    {
        const SDE::CompiledInstance* origin =
            originFor(menu.value("id", std::string{}));
        for (const Json& item : menu["items"])
        {
            const std::string kind = item.value("kind", std::string{});
            if (kind == "action")
            {
                const std::string actionId = item.value("actionId", std::string{});
                if (actionIds.find(actionId) == actionIds.end())
                {
                    AddWriterDiagnostic(diagnostics,
                                        SDE::DiagnosticCategory::Reference,
                                        origin,
                                        "SDE_EDITOR_COMPOSITION_UNKNOWN_ACTION",
                                        "Menu references an unknown editor action.",
                                        actionId);
                }
            }
            else if (kind == "submenu")
            {
                const std::string menuId = item.value("menuId", std::string{});
                if (menuIds.find(menuId) == menuIds.end())
                {
                    AddWriterDiagnostic(diagnostics,
                                        SDE::DiagnosticCategory::Reference,
                                        origin,
                                        "SDE_EDITOR_COMPOSITION_UNKNOWN_MENU",
                                        "Menu references an unknown submenu.",
                                        menuId);
                }
            }
        }
    }

    for (const Json& workspace : artifact["workspaces"])
    {
        const SDE::CompiledInstance* origin =
            originFor(workspace.value("id", std::string{}));
        const std::string layoutId = workspace.value("layoutId", std::string{});
        if (!layoutId.empty() && layoutIds.find(layoutId) == layoutIds.end())
        {
            AddWriterDiagnostic(diagnostics,
                                SDE::DiagnosticCategory::Reference,
                                origin,
                                "SDE_EDITOR_COMPOSITION_UNKNOWN_LAYOUT",
                                "Workspace references an unknown layout.",
                                layoutId);
        }
        for (const Json& panelId : workspace["visiblePanels"])
        {
            const std::string id = panelId.get<std::string>();
            if (panelIds.find(id) == panelIds.end())
            {
                AddWriterDiagnostic(diagnostics,
                                    SDE::DiagnosticCategory::Reference,
                                    origin,
                                    "SDE_EDITOR_COMPOSITION_UNKNOWN_PANEL",
                                    "Workspace references an unknown panel.",
                                    id);
            }
        }
    }

    for (const Json& mode : artifact["editorModes"])
    {
        const SDE::CompiledInstance* origin =
            originFor(mode.value("id", std::string{}));
        const std::string workspaceId = mode.value("workspaceId", std::string{});
        if (workspaceIds.find(workspaceId) == workspaceIds.end())
        {
            AddWriterDiagnostic(diagnostics,
                                SDE::DiagnosticCategory::Reference,
                                origin,
                                "SDE_EDITOR_COMPOSITION_UNKNOWN_WORKSPACE",
                                "Editor mode references an unknown workspace.",
                                workspaceId);
        }
        for (const Json& panelId : mode["requiredPanels"])
        {
            const std::string id = panelId.get<std::string>();
            if (panelIds.find(id) == panelIds.end())
            {
                AddWriterDiagnostic(diagnostics,
                                    SDE::DiagnosticCategory::Reference,
                                    origin,
                                    "SDE_EDITOR_COMPOSITION_UNKNOWN_PANEL",
                                    "Editor mode references an unknown panel.",
                                    id);
            }
        }
    }
}

[[nodiscard]] Json DiagnosticJson(const SDE::Diagnostic& diagnostic)
{
    return {
        {"severity", SDE::SeverityName(diagnostic.severity)},
        {"category", SDE::DiagnosticCategoryName(diagnostic.category)},
        {"code", diagnostic.code},
        {"message", diagnostic.message},
        {"file", diagnostic.location.file},
        {"line", diagnostic.location.line},
        {"column", diagnostic.location.column},
        {"metadata", diagnostic.metadata},
    };
}

[[nodiscard]] Json DiagnosticsJson(const std::vector<SDE::Diagnostic>& diagnostics)
{
    Json array = Json::array();
    for (const SDE::Diagnostic& diagnostic : diagnostics)
    {
        array.push_back(DiagnosticJson(diagnostic));
    }
    return array;
}

[[nodiscard]] Json DependencyRecordsJson(
    const std::vector<SDE::DependencyRecord>& records)
{
    Json array = Json::array();
    for (const SDE::DependencyRecord& record : records)
    {
        array.push_back({
            {"logicalId", record.logicalId},
            {"normalizedPath", record.normalizedPath},
            {"fingerprint", record.fingerprint},
        });
    }
    return array;
}

[[nodiscard]] Json DependenciesJson(const SDE::DependencyManifest& manifest)
{
    Json edges = Json::array();
    for (const SDE::DependencyEdge& edge : manifest.edges)
    {
        edges.push_back({
            {"fromLogicalId", edge.fromLogicalId},
            {"toLogicalId", edge.toLogicalId},
        });
    }
    return {
        {"directSchemas", DependencyRecordsJson(manifest.directSchemas)},
        {"transitiveSchemas", DependencyRecordsJson(manifest.transitiveSchemas)},
        {"dataFiles", DependencyRecordsJson(manifest.dataFiles)},
        {"edges", std::move(edges)},
    };
}

bool WriteTextFile(const std::filesystem::path& path,
                   const std::string& text,
                   std::vector<SDE::Diagnostic>& diagnostics)
{
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open())
    {
        diagnostics.push_back(MakeDiagnostic(
            SDE::Severity::Fatal,
            SDE::DiagnosticCategory::IO,
            {path.string(), 0, 0},
            "SDE_EDITOR_COMPOSITION_WRITE_FAILED",
            "Could not open editor composition output file."));
        return false;
    }
    output << text;
    if (!output.good())
    {
        diagnostics.push_back(MakeDiagnostic(
            SDE::Severity::Fatal,
            SDE::DiagnosticCategory::IO,
            {path.string(), 0, 0},
            "SDE_EDITOR_COMPOSITION_WRITE_FAILED",
            "Could not write editor composition output file."));
        return false;
    }
    return true;
}

[[nodiscard]] Json BuildArtifact(const SDE::CompiledModelGraph& graph,
                                 const SDE::CompiledInstance& composition,
                                 Json& sourceMap,
                                 std::map<std::string, const SDE::CompiledInstance*>& origins,
                                 std::vector<SDE::Diagnostic>& diagnostics)
{
    Json artifact = {
        {"artifactKind", kArtifactKind},
        {"artifactVersion", kArtifactVersion},
        {"schemaPackage", kSchemaPackage},
        {"schemaPackageVersion", kSchemaPackageVersion},
        {"compositionId", composition.instanceId},
        {"sourceMapRef", kSourceMapFile},
        {"metadata", {
            {"displayName", TextField(composition, "displayName", composition.instanceId)},
            {"description", TextField(composition, "description")},
        }},
        {"panels", Json::array()},
        {"actions", Json::array()},
        {"menus", Json::array()},
        {"toolbars", Json::array()},
        {"shortcuts", Json::array()},
        {"workspaceLayouts", Json::array()},
        {"workspaces", Json::array()},
        {"editorModes", Json::array()},
    };

    ValidateRequiredText(diagnostics, composition, "displayName",
                         artifact["metadata"].value("displayName", std::string{}));

    for (const SDE::CompiledInstance* panel :
         InstancesForComposition(graph, "EditorPanel", composition.instanceId))
    {
        Json entry = {
            {"id", panel->instanceId},
            {"displayName", TextField(*panel, "displayName", panel->instanceId)},
            {"kind", TextField(*panel, "kind")},
            {"defaultVisible", BoolField(*panel, "defaultVisible", true)},
            {"internalOnly", BoolField(*panel, "internalOnly", false)},
            {"allowedProfiles", TextListJson(TextListField(graph, *panel, "allowedProfiles"))},
        };
        origins[panel->instanceId] = panel;
        ValidateRequiredText(diagnostics, *panel, "displayName", entry.value("displayName", std::string{}));
        artifact["panels"].push_back(std::move(entry));
        AddSourceMapEntry(sourceMap,
                          "/panels/" + std::to_string(artifact["panels"].size() - 1),
                          *panel);
    }

    for (const SDE::CompiledInstance* action :
         InstancesForComposition(graph, "EditorAction", composition.instanceId))
    {
        Json entry = {
            {"id", action->instanceId},
            {"displayName", TextField(*action, "displayName", action->instanceId)},
            {"category", TextField(*action, "category")},
            {"safeInProduct", BoolField(*action, "safeInProduct", true)},
            {"internalOnly", BoolField(*action, "internalOnly", false)},
        };
        origins[action->instanceId] = action;
        ValidateRequiredText(diagnostics, *action, "displayName", entry.value("displayName", std::string{}));
        artifact["actions"].push_back(std::move(entry));
        AddSourceMapEntry(sourceMap,
                          "/actions/" + std::to_string(artifact["actions"].size() - 1),
                          *action);
    }

    for (const SDE::CompiledInstance* menu :
         InstancesForComposition(graph, "EditorMenu", composition.instanceId))
    {
        Json items = Json::array();
        for (const SDE::CompiledInstance* item : graph.AllOf("EditorMenuItem"))
        {
            if (RefInstanceId(*item, "menu") != menu->instanceId)
            {
                continue;
            }
            items.push_back({
                {"kind", TextField(*item, "kind")},
                {"actionId", TextField(*item, "actionId")},
                {"menuId", TextField(*item, "menuId")},
            });
        }

        Json entry = {
            {"id", menu->instanceId},
            {"displayName", TextField(*menu, "displayName", menu->instanceId)},
            {"items", std::move(items)},
        };
        origins[menu->instanceId] = menu;
        ValidateRequiredText(diagnostics, *menu, "displayName", entry.value("displayName", std::string{}));
        artifact["menus"].push_back(std::move(entry));
        AddSourceMapEntry(sourceMap,
                          "/menus/" + std::to_string(artifact["menus"].size() - 1),
                          *menu);
    }

    for (const SDE::CompiledInstance* toolbar :
         InstancesForComposition(graph, "EditorToolbar", composition.instanceId))
    {
        Json entry = {
            {"id", toolbar->instanceId},
            {"displayName", TextField(*toolbar, "displayName", toolbar->instanceId)},
            {"actions", TextListJson(TextListField(graph, *toolbar, "actions"))},
        };
        origins[toolbar->instanceId] = toolbar;
        ValidateRequiredText(diagnostics, *toolbar, "displayName", entry.value("displayName", std::string{}));
        artifact["toolbars"].push_back(std::move(entry));
        AddSourceMapEntry(sourceMap,
                          "/toolbars/" + std::to_string(artifact["toolbars"].size() - 1),
                          *toolbar);
    }

    for (const SDE::CompiledInstance* shortcut :
         InstancesForComposition(graph, "EditorShortcut", composition.instanceId))
    {
        Json entry = {
            {"id", shortcut->instanceId},
            {"actionId", TextField(*shortcut, "actionId")},
            {"chord", TextField(*shortcut, "chord")},
        };
        origins[shortcut->instanceId] = shortcut;
        ValidateRequiredText(diagnostics, *shortcut, "actionId", entry.value("actionId", std::string{}));
        ValidateRequiredText(diagnostics, *shortcut, "chord", entry.value("chord", std::string{}));
        artifact["shortcuts"].push_back(std::move(entry));
        AddSourceMapEntry(sourceMap,
                          "/shortcuts/" + std::to_string(artifact["shortcuts"].size() - 1),
                          *shortcut);
    }

    for (const SDE::CompiledInstance* layout :
         InstancesForComposition(graph, "EditorWorkspaceLayout", composition.instanceId))
    {
        Json entry = {
            {"id", layout->instanceId},
            {"displayName", TextField(*layout, "displayName", layout->instanceId)},
        };
        origins[layout->instanceId] = layout;
        ValidateRequiredText(diagnostics, *layout, "displayName", entry.value("displayName", std::string{}));
        artifact["workspaceLayouts"].push_back(std::move(entry));
        AddSourceMapEntry(sourceMap,
                          "/workspaceLayouts/" + std::to_string(artifact["workspaceLayouts"].size() - 1),
                          *layout);
    }

    for (const SDE::CompiledInstance* workspace :
         InstancesForComposition(graph, "EditorWorkspace", composition.instanceId))
    {
        Json entry = {
            {"id", workspace->instanceId},
            {"displayName", TextField(*workspace, "displayName", workspace->instanceId)},
            {"layoutId", TextField(*workspace, "layoutId")},
            {"visiblePanels", TextListJson(TextListField(graph, *workspace, "visiblePanels"))},
        };
        origins[workspace->instanceId] = workspace;
        ValidateRequiredText(diagnostics, *workspace, "displayName", entry.value("displayName", std::string{}));
        artifact["workspaces"].push_back(std::move(entry));
        AddSourceMapEntry(sourceMap,
                          "/workspaces/" + std::to_string(artifact["workspaces"].size() - 1),
                          *workspace);
    }

    for (const SDE::CompiledInstance* mode :
         InstancesForComposition(graph, "EditorMode", composition.instanceId))
    {
        Json entry = {
            {"id", mode->instanceId},
            {"displayName", TextField(*mode, "displayName", mode->instanceId)},
            {"workspaceId", TextField(*mode, "workspaceId")},
            {"requiredPanels", TextListJson(TextListField(graph, *mode, "requiredPanels"))},
        };
        origins[mode->instanceId] = mode;
        ValidateRequiredText(diagnostics, *mode, "displayName", entry.value("displayName", std::string{}));
        ValidateRequiredText(diagnostics, *mode, "workspaceId", entry.value("workspaceId", std::string{}));
        artifact["editorModes"].push_back(std::move(entry));
        AddSourceMapEntry(sourceMap,
                          "/editorModes/" + std::to_string(artifact["editorModes"].size() - 1),
                          *mode);
    }

    ValidateDuplicateIds(diagnostics, artifact["panels"], "panels");
    ValidateDuplicateIds(diagnostics, artifact["actions"], "actions");
    ValidateDuplicateIds(diagnostics, artifact["menus"], "menus");
    ValidateDuplicateIds(diagnostics, artifact["toolbars"], "toolbars");
    ValidateDuplicateIds(diagnostics, artifact["shortcuts"], "shortcuts");
    ValidateDuplicateIds(diagnostics, artifact["workspaceLayouts"], "workspaceLayouts");
    ValidateDuplicateIds(diagnostics, artifact["workspaces"], "workspaces");
    ValidateDuplicateIds(diagnostics, artifact["editorModes"], "editorModes");
    ValidateArtifactReferences(artifact, origins, diagnostics);

    return artifact;
}

[[nodiscard]] CompositionCompileResult Run(
    const CompositionCompileRequest& request,
    bool writeOutputs)
{
    CompositionCompileResult result;
    const std::filesystem::path outputRoot = ResolveOutputRoot(request);
    if (HasAnyExplicitOutput(request.explicitOutputs) &&
        !HasCompleteExplicitOutputs(request.explicitOutputs))
    {
        result.outputs = MakeOutputPaths(outputRoot);
        result.state = SDE::CompileState::IOError;
        result.diagnostics.push_back(MakeDiagnostic(
            SDE::Severity::Fatal,
            SDE::DiagnosticCategory::IO,
            {request.workspaceRoot.string(), 0, 0},
            "SDE_EDITOR_COMPOSITION_INCOMPLETE_OUTPUT_CONTRACT",
            "Explicit editor composition outputs must provide artifact, manifest, diagnostics, source map, and dependency manifest paths."));
        return result;
    }
    result.outputs = ResolveOutputPaths(request);

    SDE::CompilerFacade compiler;
    SDE::CompileRequest sdeRequest;
    sdeRequest.workspaceRoot = request.workspaceRoot;
    sdeRequest.outputRoot = outputRoot;
    sdeRequest.domain = kSchemaPackage;
    sdeRequest.artifactKind = kArtifactKind;

    SDE::CompilerFacadeResult compiled = compiler.Compile(sdeRequest);
    result.state = compiled.project.state;
    result.hashes = compiled.project.hashes;
    result.diagnostics = compiled.project.validation.diagnostics;

    if (!SDE::IsUsable(compiled.project.state) || !compiled.project.graph)
    {
        result.state = StateFromDiagnostics(result.state, result.diagnostics);
        if (writeOutputs)
        {
            WriteTextFile(result.outputs.diagnosticsPath,
                          DiagnosticsJson(result.diagnostics).dump(2),
                          result.diagnostics);
        }
        return result;
    }

    const SDE::CompiledInstance* composition = SelectComposition(
        *compiled.project.graph, request.compositionId, result.diagnostics);
    if (composition == nullptr)
    {
        result.state = StateFromDiagnostics(result.state, result.diagnostics);
        if (writeOutputs)
        {
            WriteTextFile(result.outputs.diagnosticsPath,
                          DiagnosticsJson(result.diagnostics).dump(2),
                          result.diagnostics);
        }
        return result;
    }

    Json sourceMap = Json::array();
    std::map<std::string, const SDE::CompiledInstance*> origins;
    Json artifact = BuildArtifact(*compiled.project.graph,
                                  *composition,
                                  sourceMap,
                                  origins,
                                  result.diagnostics);
    const std::string artifactText = artifact.dump(2);
    const std::string artifactHash = SDE::StableHashBytes(artifactText);
    result.hashes.artifactHash = artifactHash;

    Json manifest = {
        {"manifestKind", kManifestKind},
        {"manifestVersion", kManifestVersion},
        {"compositionId", composition->instanceId},
        {"artifactPath", ManifestPathReference(result.outputs.artifactPath, result.outputs.manifestPath)},
        {"diagnosticsPath", ManifestPathReference(result.outputs.diagnosticsPath, result.outputs.manifestPath)},
        {"sourceMapPath", ManifestPathReference(result.outputs.sourceMapPath, result.outputs.manifestPath)},
        {"dependencyManifestPath", ManifestPathReference(result.outputs.dependencyManifestPath,
                                                         result.outputs.manifestPath)},
        {"artifactHash", artifactHash},
        {"schemaPackage", kSchemaPackage},
        {"schemaPackageVersion", kSchemaPackageVersion},
        {"compilerVersion", SDE::CurrentCompilerVersion().ToString()},
    };

    result.state = StateFromDiagnostics(result.state, result.diagnostics);

    if (writeOutputs)
    {
        bool wroteAll = true;
        wroteAll = WriteTextFile(result.outputs.artifactPath, artifactText, result.diagnostics) && wroteAll;
        wroteAll = WriteTextFile(result.outputs.manifestPath, manifest.dump(2), result.diagnostics) && wroteAll;
        wroteAll = WriteTextFile(result.outputs.diagnosticsPath,
                                 DiagnosticsJson(result.diagnostics).dump(2),
                                 result.diagnostics) && wroteAll;
        wroteAll = WriteTextFile(result.outputs.sourceMapPath, sourceMap.dump(2), result.diagnostics) && wroteAll;
        wroteAll = WriteTextFile(result.outputs.dependencyManifestPath,
                                 DependenciesJson(compiled.project.dependencies).dump(2),
                                 result.diagnostics) && wroteAll;
        if (!wroteAll)
        {
            result.state = SDE::Merge(result.state, SDE::CompileState::IOError);
        }
    }

    return result;
}

} // namespace

CompositionOutputPaths MakeBuildOutputPaths(const std::filesystem::path& buildRoot)
{
    return {
        buildRoot / "Artifacts" / "EditorComposition" / kArtifactFile,
        buildRoot / "Manifests" / kManifestFile,
        buildRoot / "Reports" / kDiagnosticsFile,
        buildRoot / "Manifests" / kSourceMapFile,
        buildRoot / "Manifests" / kDependencyFile,
    };
}

CompositionCompileResult CompositionCompiler::Validate(
    const CompositionCompileRequest& request) const
{
    return Run(request, false);
}

CompositionCompileResult CompositionCompiler::Compile(
    const CompositionCompileRequest& request) const
{
    return Run(request, true);
}

} // namespace SagaEditorComposition
