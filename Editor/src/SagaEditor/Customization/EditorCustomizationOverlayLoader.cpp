/// @file EditorCustomizationOverlayLoader.cpp
/// @brief JSON loader for safe editor customization overlays.

#include "SagaEditor/Customization/EditorCustomizationOverlayLoader.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace SagaEditor
{
namespace
{

using Json = nlohmann::json;

[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& path,
                                       std::vector<EditorCompositionDiagnostic>& diagnostics)
{
    std::ifstream input(path);
    if (!input)
    {
        diagnostics.push_back({ EditorCompositionDiagnosticSeverity::Blocker,
                                "CompositionOverlay.FileOpenFailed",
                                "Could not open editor customization overlay file.",
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
                          "CompositionOverlay.InvalidJsonRoot",
                          "Editor customization overlay root must be an object.",
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
                      "CompositionOverlay.JsonParseFailed",
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
                          "CompositionOverlay.MissingField",
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
                      "CompositionOverlay.InvalidFieldType",
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
                               std::vector<EditorCompositionDiagnostic>& diagnostics)
{
    auto it = object.find(key);
    if (it == object.end() || it->is_null())
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Error,
                      "CompositionOverlay.MissingField",
                      std::string("Missing required field: ") + key,
                      documentPath,
                      jsonPointer + "/" + key);
        return 0;
    }

    if (!it->is_number_unsigned())
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Error,
                      "CompositionOverlay.InvalidFieldType",
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
                      "CompositionOverlay.InvalidFieldType",
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
                      "CompositionOverlay.InvalidFieldType",
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
                          "CompositionOverlay.InvalidFieldType",
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
[[nodiscard]] std::vector<T> ParseArray(
    const Json& root,
    const char* key,
    const std::string& documentPath,
    std::vector<EditorCompositionDiagnostic>& diagnostics,
    ParseFn parse)
{
    std::vector<T> values;
    auto it = root.find(key);
    if (it == root.end() || it->is_null())
    {
        return values;
    }

    if (!it->is_array())
    {
        AddDiagnostic(diagnostics,
                      EditorCompositionDiagnosticSeverity::Error,
                      "CompositionOverlay.InvalidFieldType",
                      std::string("Expected array field: ") + key,
                      documentPath,
                      std::string("/") + key);
        return values;
    }

    for (std::size_t index = 0; index < it->size(); ++index)
    {
        const Json& item = (*it)[index];
        const std::string pointer = std::string("/") + key + "/" + std::to_string(index);
        if (!item.is_object())
        {
            AddDiagnostic(diagnostics,
                          EditorCompositionDiagnosticSeverity::Error,
                          "CompositionOverlay.InvalidOverride",
                          "Customization override entries must be objects.",
                          documentPath,
                          pointer);
            continue;
        }
        values.push_back(parse(item, pointer));
    }

    return values;
}

} // namespace

EditorCustomizationOverlayLoadResult EditorCustomizationOverlayLoader::LoadFile(
    const std::filesystem::path& path) const
{
    EditorCustomizationOverlayLoadResult result;
    const std::string text = ReadTextFile(path, result.diagnostics);
    if (text.empty() && HasBlockingCompositionDiagnostic(result.diagnostics))
    {
        return result;
    }

    return LoadText(text, path.string());
}

EditorCustomizationOverlayLoadResult EditorCustomizationOverlayLoader::LoadText(
    const std::string& text,
    std::string documentPath) const
{
    EditorCustomizationOverlayLoadResult result;
    std::optional<Json> root = ParseJson(text, documentPath, result.diagnostics);
    if (!root)
    {
        return result;
    }

    EditorCustomizationOverlay overlay;
    overlay.schemaVersion = GetUInt(*root, "schemaVersion", documentPath, "", result.diagnostics);
    overlay.baseCompositionId = GetString(*root, "baseCompositionId", documentPath, "", result.diagnostics);
    overlay.baseArtifactHash = GetString(*root, "baseArtifactHash", documentPath, "", result.diagnostics, false);
    overlay.overlayId = GetString(*root, "overlayId", documentPath, "", result.diagnostics);

    if (overlay.schemaVersion > kCurrentEditorCustomizationOverlayVersion)
    {
        AddDiagnostic(result.diagnostics,
                      EditorCompositionDiagnosticSeverity::Blocker,
                      "CompositionOverlay.UnsupportedSchemaVersion",
                      "Overlay schema version is newer than this editor supports.",
                      documentPath,
                      "/schemaVersion");
    }

    overlay.layoutOverrides = ParseArray<LayoutCustomizationOverride>(
        *root,
        "layoutOverrides",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            LayoutCustomizationOverride overrideValue;
            overrideValue.workspaceId = GetString(item, "workspaceId", documentPath, pointer, result.diagnostics);
            overrideValue.panelId = GetString(item, "panelId", documentPath, pointer, result.diagnostics);
            overrideValue.placement = GetString(item, "placement", documentPath, pointer, result.diagnostics, false);
            overrideValue.visible = GetBool(item, "visible", true, documentPath, pointer, result.diagnostics);
            return overrideValue;
        });

    overlay.shortcutOverrides = ParseArray<ShortcutCustomizationOverride>(
        *root,
        "shortcutOverrides",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            ShortcutCustomizationOverride overrideValue;
            overrideValue.actionId = GetString(item, "actionId", documentPath, pointer, result.diagnostics);
            overrideValue.chord = GetString(item, "chord", documentPath, pointer, result.diagnostics);
            return overrideValue;
        });

    overlay.visibilityOverrides = ParseArray<VisibilityCustomizationOverride>(
        *root,
        "visibilityOverrides",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            VisibilityCustomizationOverride overrideValue;
            overrideValue.panelId = GetString(item, "panelId", documentPath, pointer, result.diagnostics);
            overrideValue.visible = GetBool(item, "visible", true, documentPath, pointer, result.diagnostics);
            return overrideValue;
        });

    overlay.toolbarOverrides = ParseArray<ToolbarCustomizationOverride>(
        *root,
        "toolbarOverrides",
        documentPath,
        result.diagnostics,
        [&](const Json& item, const std::string& pointer)
        {
            ToolbarCustomizationOverride overrideValue;
            overrideValue.toolbarId = GetString(item, "toolbarId", documentPath, pointer, result.diagnostics);
            overrideValue.hiddenActionIds = GetStringArray(item, "hiddenActionIds", documentPath, pointer, result.diagnostics);
            return overrideValue;
        });

    result.overlay = std::move(overlay);
    return result;
}

} // namespace SagaEditor
