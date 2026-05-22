/// @file EditorCompositionArtifactLoader.cpp
/// @brief JSON loader for SDE-emitted editor composition artifacts and manifests.

#include "SagaEditor/Composition/EditorCompositionArtifactLoader.h"

#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace SagaEditor
{
namespace
{

using Json = nlohmann::json;

constexpr const char* kExpectedArtifactKind = "saga.editor.composition";
constexpr const char* kExpectedManifestKind = "saga.editor.composition.manifest";
constexpr const char* kExpectedSchemaPackage = "saga.editor";

void HashAppend(uint64_t& hash, const std::string& bytes)
{
    constexpr uint64_t kPrime = 1099511628211ull;
    for (unsigned char c : bytes)
    {
        hash ^= static_cast<uint64_t>(c);
        hash *= kPrime;
    }
}

[[nodiscard]] std::string Hex64(uint64_t value)
{
    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << value;
    return output.str();
}

[[nodiscard]] std::string StableHashBytes(const std::string& bytes)
{
    uint64_t hash = 14695981039346656037ull;
    HashAppend(hash, bytes);
    return Hex64(hash);
}

[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& path,
                                       std::vector<EditorCompositionDiagnostic>& diagnostics)
{
    std::ifstream input(path);
    if (!input)
    {
        diagnostics.push_back({ EditorCompositionDiagnosticSeverity::Blocker,
                                "CompositionArtifact.FileOpenFailed",
                                "Could not open editor composition file.",
                                path.string(),
                                {},
                                {} });
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void AddDiagnostic(std::vector<EditorCompositionDiagnostic>& diagnostics,
                   EditorCompositionDiagnosticSeverity severity,
                   std::string code,
                   std::string message,
                   const std::string& documentPath,
                   std::string jsonPointer,
                   std::string referencedId = {})
{
    diagnostics.push_back({ severity,
                            std::move(code),
                            std::move(message),
                            documentPath,
                            std::move(jsonPointer),
                            std::move(referencedId) });
}

[[nodiscard]] std::optional<Json> ParseJson(
    const std::string& text,
    const std::string& documentPath,
    std::vector<EditorCompositionDiagnostic>& diagnostics)
{
    try
    {
        Json parsed = Json::parse(text);
        if (!parsed.is_object())
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Blocker,
                          "CompositionArtifact.InvalidJsonRoot",
                          "Editor composition JSON root must be an object.",
                          documentPath,
                          {});
            return std::nullopt;
        }
        return parsed;
    }
    catch (const Json::parse_error& error)
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.JsonParseFailed",
                      error.what(),
                      documentPath,
                      {});
        return std::nullopt;
    }
}

[[nodiscard]] std::string GetString(const Json& object,
                                    const char* key,
                                    const std::string& documentPath,
                                    const std::string& jsonPointer,
                                    std::vector<EditorCompositionDiagnostic>& diagnostics,
                                    bool required = true)
{
    auto it = object.find(key);
    if (it == object.end() || it->is_null())
    {
        if (required)
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.MissingField",
                          std::string("Missing required field: ") + key,
                          documentPath,
                          jsonPointer + "/" + key);
        }
        return {};
    }

    if (!it->is_string())
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Error,
                      "CompositionArtifact.InvalidFieldType",
                      std::string("Expected string field: ") + key,
                      documentPath,
                      jsonPointer + "/" + key);
        return {};
    }

    return it->get<std::string>();
}

[[nodiscard]] uint32_t GetUInt(const Json& object,
                               const char* key,
                               const std::string& documentPath,
                               const std::string& jsonPointer,
                               std::vector<EditorCompositionDiagnostic>& diagnostics,
                               bool required = true)
{
    auto it = object.find(key);
    if (it == object.end() || it->is_null())
    {
        if (required)
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.MissingField",
                          std::string("Missing required field: ") + key,
                          documentPath,
                          jsonPointer + "/" + key);
        }
        return 0;
    }

    if (!it->is_number_unsigned())
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Error,
                      "CompositionArtifact.InvalidFieldType",
                      std::string("Expected unsigned integer field: ") + key,
                      documentPath,
                      jsonPointer + "/" + key);
        return 0;
    }

    return it->get<uint32_t>();
}

[[nodiscard]] bool GetBool(const Json& object,
                           const char* key,
                           bool fallback,
                           const std::string& documentPath,
                           const std::string& jsonPointer,
                           std::vector<EditorCompositionDiagnostic>& diagnostics)
{
    auto it = object.find(key);
    if (it == object.end() || it->is_null())
    {
        return fallback;
    }

    if (!it->is_boolean())
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Error,
                      "CompositionArtifact.InvalidFieldType",
                      std::string("Expected boolean field: ") + key,
                      documentPath,
                      jsonPointer + "/" + key);
        return fallback;
    }

    return it->get<bool>();
}

[[nodiscard]] std::vector<std::string> GetStringArray(
    const Json& object,
    const char* key,
    const std::string& documentPath,
    const std::string& jsonPointer,
    std::vector<EditorCompositionDiagnostic>& diagnostics)
{
    std::vector<std::string> values;
    auto it = object.find(key);
    if (it == object.end() || it->is_null())
    {
        return values;
    }

    if (!it->is_array())
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Error,
                      "CompositionArtifact.InvalidFieldType",
                      std::string("Expected array field: ") + key,
                      documentPath,
                      jsonPointer + "/" + key);
        return values;
    }

    for (std::size_t index = 0; index < it->size(); ++index)
    {
        const Json& item = (*it)[index];
        if (!item.is_string())
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.InvalidFieldType",
                          "Expected string array item.",
                          documentPath,
                          jsonPointer + "/" + key + "/" + std::to_string(index));
            continue;
        }
        values.push_back(item.get<std::string>());
    }

    return values;
}

template <typename T, typename ParseFn>
[[nodiscard]] std::vector<T> ParseDefinitionArray(
    const Json& root,
    const char* key,
    const std::string& documentPath,
    std::vector<EditorCompositionDiagnostic>& diagnostics,
    ParseFn parse)
{
    std::vector<T> definitions;
    auto it = root.find(key);
    if (it == root.end() || it->is_null())
    {
        return definitions;
    }

    if (!it->is_array())
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Error,
                      "CompositionArtifact.InvalidFieldType",
                      std::string("Expected array field: ") + key,
                      documentPath,
                      std::string("/") + key);
        return definitions;
    }

    for (std::size_t index = 0; index < it->size(); ++index)
    {
        const Json& item = (*it)[index];
        const std::string itemPointer = std::string("/") + key + "/" + std::to_string(index);
        if (!item.is_object())
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionArtifact.InvalidDefinition",
                          "Composition definition entries must be objects.",
                          documentPath,
                          itemPointer);
            continue;
        }
        definitions.push_back(parse(item, itemPointer));
    }

    return definitions;
}

} // namespace

EditorCompositionManifestLoadResult EditorCompositionArtifactLoader::LoadManifestFile(
    const std::filesystem::path& path) const
{
    EditorCompositionManifestLoadResult result;
    const std::string text = ReadTextFile(path, result.diagnostics);
    if (text.empty() && HasBlockingCompositionDiagnostic(result.diagnostics))
    {
        return result;
    }

    return LoadManifestText(text, path.string());
}

EditorCompositionManifestLoadResult EditorCompositionArtifactLoader::LoadManifestText(
    const std::string& text,
    std::string documentPath) const
{
    EditorCompositionManifestLoadResult result;
    std::optional<Json> root = ParseJson(text, documentPath, result.diagnostics);
    if (!root)
    {
        return result;
    }

    EditorCompositionManifest manifest;
    manifest.manifestKind = GetString(*root, "manifestKind", documentPath, "", result.diagnostics);
    manifest.manifestVersion = GetUInt(*root, "manifestVersion", documentPath, "", result.diagnostics);
    manifest.compositionId = GetString(*root, "compositionId", documentPath, "", result.diagnostics);
    manifest.artifactPath = GetString(*root, "artifactPath", documentPath, "", result.diagnostics);
    manifest.diagnosticsPath = GetString(*root, "diagnosticsPath", documentPath, "", result.diagnostics, false);
    manifest.sourceMapPath = GetString(*root, "sourceMapPath", documentPath, "", result.diagnostics, false);
    manifest.dependencyManifestPath = GetString(*root, "dependencyManifestPath", documentPath, "", result.diagnostics, false);
    manifest.artifactHash = GetString(*root, "artifactHash", documentPath, "", result.diagnostics, false);
    manifest.schemaPackage = GetString(*root, "schemaPackage", documentPath, "", result.diagnostics);
    manifest.schemaPackageVersion = GetUInt(*root, "schemaPackageVersion", documentPath, "", result.diagnostics);
    manifest.compilerVersion = GetString(*root, "compilerVersion", documentPath, "", result.diagnostics, false);

    if (manifest.manifestKind != kExpectedManifestKind)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.InvalidManifestKind",
                      "Manifest is not a Saga editor composition manifest.",
                      documentPath,
                      "/manifestKind",
                      manifest.manifestKind);
    }

    if (manifest.manifestVersion > kCurrentEditorCompositionManifestVersion)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.UnsupportedManifestVersion",
                      "Manifest version is newer than this editor supports.",
                      documentPath,
                      "/manifestVersion");
    }

    if (manifest.schemaPackage != kExpectedSchemaPackage)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.InvalidSchemaPackage",
                      "Manifest was not compiled with the saga.editor schema package.",
                      documentPath,
                      "/schemaPackage",
                      manifest.schemaPackage);
    }

    result.manifest = manifest;
    return result;
}

EditorCompositionArtifactLoadResult EditorCompositionArtifactLoader::LoadArtifactFile(
    const std::filesystem::path& path) const
{
    EditorCompositionArtifactLoadResult result;
    const std::string text = ReadTextFile(path, result.diagnostics);
    if (text.empty() && HasBlockingCompositionDiagnostic(result.diagnostics))
    {
        return result;
    }

    return LoadArtifactText(text, path.string());
}

EditorCompositionArtifactLoadResult EditorCompositionArtifactLoader::LoadArtifactText(
    const std::string& text,
    std::string documentPath) const
{
    EditorCompositionArtifactLoadResult result;
    std::optional<Json> root = ParseJson(text, documentPath, result.diagnostics);
    if (!root)
    {
        return result;
    }

    EditorCompositionArtifact artifact;
    artifact.artifactKind = GetString(*root, "artifactKind", documentPath, "", result.diagnostics);
    artifact.artifactVersion = GetUInt(*root, "artifactVersion", documentPath, "", result.diagnostics);
    artifact.schemaPackage = GetString(*root, "schemaPackage", documentPath, "", result.diagnostics);
    artifact.schemaPackageVersion = GetUInt(*root, "schemaPackageVersion", documentPath, "", result.diagnostics);
    artifact.compositionId = GetString(*root, "compositionId", documentPath, "", result.diagnostics);
    artifact.sourceMapRef = GetString(*root, "sourceMapRef", documentPath, "", result.diagnostics, false);

    if (auto metadataIt = root->find("metadata"); metadataIt != root->end() && metadataIt->is_object())
    {
        artifact.metadata.displayName = GetString(*metadataIt, "displayName", documentPath, "/metadata", result.diagnostics, false);
        artifact.metadata.description = GetString(*metadataIt, "description", documentPath, "/metadata", result.diagnostics, false);
    }

    artifact.panels = ParseDefinitionArray<PanelDefinition>(
        *root,
        "panels",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            PanelDefinition panel;
            panel.id = GetString(item, "id", documentPath, pointer, result.diagnostics);
            panel.displayName = GetString(item, "displayName", documentPath, pointer, result.diagnostics);
            panel.kind = GetString(item, "kind", documentPath, pointer, result.diagnostics, false);
            panel.defaultVisible = GetBool(item, "defaultVisible", true, documentPath, pointer, result.diagnostics);
            panel.internalOnly = GetBool(item, "internalOnly", false, documentPath, pointer, result.diagnostics);
            panel.allowedProfiles = GetStringArray(item, "allowedProfiles", documentPath, pointer, result.diagnostics);
            return panel;
        });

    artifact.actions = ParseDefinitionArray<ActionDefinition>(
        *root,
        "actions",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            ActionDefinition action;
            action.id = GetString(item, "id", documentPath, pointer, result.diagnostics);
            action.displayName = GetString(item, "displayName", documentPath, pointer, result.diagnostics);
            action.category = GetString(item, "category", documentPath, pointer, result.diagnostics, false);
            action.safeInProduct = GetBool(item, "safeInProduct", true, documentPath, pointer, result.diagnostics);
            action.internalOnly = GetBool(item, "internalOnly", false, documentPath, pointer, result.diagnostics);
            return action;
        });

    artifact.menus = ParseDefinitionArray<MenuDefinition>(
        *root,
        "menus",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            MenuDefinition menu;
            menu.id = GetString(item, "id", documentPath, pointer, result.diagnostics);
            menu.displayName = GetString(item, "displayName", documentPath, pointer, result.diagnostics);
            auto itemsIt = item.find("items");
            if (itemsIt != item.end() && itemsIt->is_array())
            {
                for (std::size_t index = 0; index < itemsIt->size(); ++index)
                {
                    const Json& menuItemJson = (*itemsIt)[index];
                    const std::string itemPointer = pointer + "/items/" + std::to_string(index);
                    if (!menuItemJson.is_object())
                    {
                        AddDiagnostic(result.diagnostics,
                                      EditorCompositionDiagnosticSeverity::Error,
                                      "CompositionArtifact.InvalidDefinition",
                                      "Menu item entries must be objects.",
                                      documentPath,
                                      itemPointer);
                        continue;
                    }
                    MenuItemDefinition menuItem;
                    menuItem.kind = GetString(menuItemJson, "kind", documentPath, itemPointer, result.diagnostics);
                    menuItem.actionId = GetString(menuItemJson, "actionId", documentPath, itemPointer, result.diagnostics, false);
                    menuItem.menuId = GetString(menuItemJson, "menuId", documentPath, itemPointer, result.diagnostics, false);
                    menu.items.push_back(std::move(menuItem));
                }
            }
            return menu;
        });

    artifact.toolbars = ParseDefinitionArray<ToolbarDefinition>(
        *root,
        "toolbars",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            ToolbarDefinition toolbar;
            toolbar.id = GetString(item, "id", documentPath, pointer, result.diagnostics);
            toolbar.displayName = GetString(item, "displayName", documentPath, pointer, result.diagnostics);
            toolbar.actions = GetStringArray(item, "actions", documentPath, pointer, result.diagnostics);
            return toolbar;
        });

    artifact.shortcuts = ParseDefinitionArray<ShortcutBindingDefinition>(
        *root,
        "shortcuts",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            ShortcutBindingDefinition shortcut;
            shortcut.id = GetString(item, "id", documentPath, pointer, result.diagnostics);
            shortcut.actionId = GetString(item, "actionId", documentPath, pointer, result.diagnostics);
            shortcut.chord = GetString(item, "chord", documentPath, pointer, result.diagnostics);
            return shortcut;
        });

    artifact.workspaceLayouts = ParseDefinitionArray<WorkspaceLayoutDefinition>(
        *root,
        "workspaceLayouts",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            WorkspaceLayoutDefinition layout;
            layout.id = GetString(item, "id", documentPath, pointer, result.diagnostics);
            layout.displayName = GetString(item, "displayName", documentPath, pointer, result.diagnostics);
            return layout;
        });

    artifact.workspaces = ParseDefinitionArray<WorkspaceProfileDefinition>(
        *root,
        "workspaces",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            WorkspaceProfileDefinition workspace;
            workspace.id = GetString(item, "id", documentPath, pointer, result.diagnostics);
            workspace.displayName = GetString(item, "displayName", documentPath, pointer, result.diagnostics);
            workspace.layoutId = GetString(item, "layoutId", documentPath, pointer, result.diagnostics, false);
            workspace.visiblePanels = GetStringArray(item, "visiblePanels", documentPath, pointer, result.diagnostics);
            return workspace;
        });

    artifact.editorModes = ParseDefinitionArray<EditorModeDefinition>(
        *root,
        "editorModes",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            EditorModeDefinition mode;
            mode.id = GetString(item, "id", documentPath, pointer, result.diagnostics);
            mode.displayName = GetString(item, "displayName", documentPath, pointer, result.diagnostics);
            mode.workspaceId = GetString(item, "workspaceId", documentPath, pointer, result.diagnostics);
            mode.requiredPanels = GetStringArray(item, "requiredPanels", documentPath, pointer, result.diagnostics);
            return mode;
        });

    if (artifact.artifactKind != kExpectedArtifactKind)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.InvalidArtifactKind",
                      "Artifact is not a Saga editor composition artifact.",
                      documentPath,
                      "/artifactKind",
                      artifact.artifactKind);
    }

    if (artifact.artifactVersion > kCurrentEditorCompositionArtifactVersion)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.UnsupportedArtifactVersion",
                      "Artifact version is newer than this editor supports.",
                      documentPath,
                      "/artifactVersion");
    }

    if (artifact.schemaPackage != kExpectedSchemaPackage)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.InvalidSchemaPackage",
                      "Artifact was not compiled with the saga.editor schema package.",
                      documentPath,
                      "/schemaPackage",
                      artifact.schemaPackage);
    }

    result.artifact = std::move(artifact);
    return result;
}

EditorCompositionLoadResult EditorCompositionArtifactLoader::LoadFromManifestFile(
    const std::filesystem::path& path) const
{
    EditorCompositionLoadResult result;
    EditorCompositionManifestLoadResult manifestResult = LoadManifestFile(path);
    result.diagnostics.insert(result.diagnostics.end(),
                              manifestResult.diagnostics.begin(),
                              manifestResult.diagnostics.end());
    if (!manifestResult.manifest || HasBlockingCompositionDiagnostic(result.diagnostics))
    {
        return result;
    }

    result.manifest = manifestResult.manifest;

    std::filesystem::path artifactPath = result.manifest->artifactPath;
    if (artifactPath.is_relative())
    {
        artifactPath = path.parent_path() / artifactPath;
    }

    const std::string artifactText = ReadTextFile(artifactPath, result.diagnostics);
    if (artifactText.empty() && HasBlockingCompositionDiagnostic(result.diagnostics))
    {
        return result;
    }

    if (!result.manifest->artifactHash.empty())
    {
        const std::string actualHash = StableHashBytes(artifactText);
        if (actualHash != result.manifest->artifactHash)
        {
            AddDiagnostic(result.diagnostics,
                          EditorCompositionDiagnosticSeverity::Blocker,
                          "CompositionArtifact.ArtifactHashMismatch",
                          "Manifest artifact hash does not match the artifact file.",
                          path.string(),
                          "/artifactHash",
                          actualHash);
        }
    }

    EditorCompositionArtifactLoadResult artifactResult =
        LoadArtifactText(artifactText, artifactPath.string());
    result.diagnostics.insert(result.diagnostics.end(),
                              artifactResult.diagnostics.begin(),
                              artifactResult.diagnostics.end());
    if (!artifactResult.artifact)
    {
        return result;
    }

    if (artifactResult.artifact->compositionId != result.manifest->compositionId)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.CompositionIdMismatch",
                      "Manifest composition id does not match the artifact composition id.",
                      path.string(),
                      "/compositionId",
                      artifactResult.artifact->compositionId);
    }

    if (artifactResult.artifact->schemaPackage != result.manifest->schemaPackage)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionArtifact.SchemaPackageMismatch",
                      "Manifest schema package does not match the artifact schema package.",
                      path.string(),
                      "/schemaPackage",
                      artifactResult.artifact->schemaPackage);
    }

    result.artifact = std::move(artifactResult.artifact);
    return result;
}

} // namespace SagaEditor
