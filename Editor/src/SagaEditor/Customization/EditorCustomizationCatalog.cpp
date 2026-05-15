/// @file EditorCustomizationCatalog.cpp
/// @brief Editor customisation catalog implementation.

#include "SagaEditor/Customization/EditorCustomizationCatalog.h"

#if SAGA_WITH_SDE
#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/Compiler/CompilerFacade.h"
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <variant>

#include <nlohmann/json.hpp>

namespace SagaEditor
{

namespace
{

[[nodiscard]] std::filesystem::path ResolveWorkspaceRoot(
    const std::filesystem::path& workspaceRoot)
{
    if (!workspaceRoot.empty())
    {
        return workspaceRoot;
    }
    return std::filesystem::current_path();
}

[[nodiscard]] std::filesystem::path CustomizationRoot(
    const std::filesystem::path& workspaceRoot)
{
    const std::filesystem::path defaultSdeRoot = workspaceRoot / ".sde";
    if (std::filesystem::exists(defaultSdeRoot / "source") ||
        std::filesystem::exists(defaultSdeRoot / "schemas") ||
        std::filesystem::exists(defaultSdeRoot / "data"))
    {
        return defaultSdeRoot;
    }

    if (std::filesystem::exists(workspaceRoot / "source") ||
        std::filesystem::exists(workspaceRoot / "schemas") ||
        std::filesystem::exists(workspaceRoot / "data"))
    {
        return workspaceRoot;
    }

    const std::filesystem::path manifestPath = workspaceRoot / "saga.project.json";
    if (std::filesystem::exists(manifestPath))
    {
        std::ifstream input(manifestPath);
        if (input.is_open())
        {
            try
            {
                nlohmann::json manifest;
                input >> manifest;
                const std::string configured =
                    manifest.value("sdeRoot", std::string{".sde"});
                return std::filesystem::path(configured).is_absolute()
                    ? std::filesystem::path(configured)
                    : workspaceRoot / configured;
            }
            catch (const nlohmann::json::exception&)
            {
            }
        }
    }

    return defaultSdeRoot;
}

[[nodiscard]] std::filesystem::path UserConfigRoot()
{
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
    {
        return std::filesystem::path(xdg) / "Saga" / "editor";
    }
    if (const char* home = std::getenv("HOME"))
    {
        return std::filesystem::path(home) / ".config" / "Saga" / "editor";
    }
    return std::filesystem::temp_directory_path() / "Saga" / "editor";
}

#if SAGA_WITH_SDE

[[nodiscard]] bool HasBuiltinProfileConflict(
    const std::vector<EditorProfile>& profiles,
    std::vector<std::string>& diagnostics)
{
    const std::set<std::string> builtins = {
        MakeBasicProfile().id,
        MakeNodeEditorProfile().id,
        MakeStandardPipelineProfile().id,
        MakeAdvancedPipelineProfile().id,
        MakeCustomProfile().id,
    };
    for (const EditorProfile& profile : profiles)
    {
        if (builtins.find(profile.id) != builtins.end())
        {
            diagnostics.push_back(
                "SDE_ARTIFACT_BUILTIN_CONFLICT: project profile id conflicts with built-in profile: " +
                profile.id);
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool ValidateArtifactContract(const SDE::ArtifactManifest& manifest,
                                            std::vector<std::string>& diagnostics)
{
    bool ok = true;
    if (manifest.artifactVersion.major != SDE::CurrentArtifactFormatVersion().major)
    {
        diagnostics.push_back("SDE_ARTIFACT_VERSION_UNSUPPORTED: unsupported artifact version.");
        ok = false;
    }
    if (manifest.domain != "Saga.Editor.Customization")
    {
        diagnostics.push_back("SDE_ARTIFACT_DOMAIN_MISMATCH: expected Saga.Editor.Customization.");
        ok = false;
    }
    if (manifest.kind != "EditorCustomizationGraph")
    {
        diagnostics.push_back("SDE_ARTIFACT_KIND_MISMATCH: expected EditorCustomizationGraph.");
        ok = false;
    }
    return ok;
}

[[nodiscard]] const SDE::CompiledValue* Field(
    const SDE::CompiledInstance& instance,
    const std::string& fieldId)
{
    return instance.GetField(fieldId);
}

[[nodiscard]] std::string TextField(const SDE::CompiledInstance& instance,
                                    const std::string& fieldId,
                                    const std::string& fallback = {})
{
    if (const SDE::CompiledValue* value = Field(instance, fieldId))
    {
        if (const auto* text = std::get_if<SDE::CompiledText>(&value->data))
        {
            return *text;
        }
    }
    return fallback;
}

[[nodiscard]] bool BoolField(const SDE::CompiledInstance& instance,
                             const std::string& fieldId,
                             bool fallback)
{
    if (const SDE::CompiledValue* value = Field(instance, fieldId))
    {
        if (const auto* boolean = std::get_if<SDE::CompiledBool>(&value->data))
        {
            return *boolean;
        }
    }
    return fallback;
}

[[nodiscard]] std::vector<std::string> TextListField(
    const SDE::CompiledModelGraph& graph,
    const SDE::CompiledInstance& instance,
    const std::string& fieldId)
{
    std::vector<std::string> out;
    const SDE::CompiledValue* value = Field(instance, fieldId);
    if (value == nullptr)
    {
        return out;
    }

    const auto* array = std::get_if<SDE::CompiledArrayRef>(&value->data);
    if (array == nullptr)
    {
        return out;
    }

    const SDE::ValueSlab& slab = graph.Slab();
    for (std::uint32_t i = 0; i < array->range.count; ++i)
    {
        const SDE::CompiledValue& element =
            slab.At(array->range.offset + i);
        if (const auto* text = std::get_if<SDE::CompiledText>(&element.data))
        {
            out.push_back(*text);
        }
    }
    return out;
}

[[nodiscard]] std::vector<std::string> Split(const std::string& text, char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, delimiter))
    {
        parts.push_back(part);
    }
    return parts;
}

[[nodiscard]] KeyChord ParseChord(const std::string& text)
{
    KeyChord chord;
    for (const std::string& token : Split(text, '+'))
    {
        if (token == "Ctrl")
        {
            chord.modifiers |= 1u;
        }
        else if (token == "Shift")
        {
            chord.modifiers |= 2u;
        }
        else if (token == "Alt")
        {
            chord.modifiers |= 4u;
        }
        else if (token == "Super")
        {
            chord.modifiers |= 8u;
        }
        else if (!token.empty())
        {
            chord.keyCode = static_cast<std::uint32_t>(token.front());
        }
    }
    return chord;
}

[[nodiscard]] std::vector<EditorShortcutBinding> ShortcutBindings(
    const SDE::CompiledModelGraph& graph,
    const SDE::CompiledInstance& instance)
{
    std::vector<EditorShortcutBinding> out;
    for (const std::string& encoded :
         TextListField(graph, instance, "shortcutBindings"))
    {
        const std::vector<std::string> parts = Split(encoded, '|');
        if (parts.size() < 2)
        {
            continue;
        }

        EditorShortcutBinding binding;
        binding.chord = ParseChord(parts[0]);
        binding.commandId = parts[1];
        binding.displayText = parts.size() >= 3 ? parts[2] : parts[0];
        if (binding.chord.keyCode != 0 && !binding.commandId.empty())
        {
            out.push_back(std::move(binding));
        }
    }
    return out;
}

[[nodiscard]] EditorProfile ProfileFromInstance(
    const SDE::CompiledModelGraph& graph,
    const SDE::CompiledInstance& instance)
{
    EditorProfile profile;
    profile.id = instance.instanceId;
    profile.displayName = TextField(instance, "displayName", profile.id);
    profile.description = TextField(instance, "description");
    profile.layoutPresetId = TextField(instance, "layoutPresetId", "custom");
    profile.shortcutMapId = TextField(instance, "shortcutMapId", "saga.shortcuts.custom");
    profile.defaultPanels = TextListField(graph, instance, "defaultPanels");
    profile.defaultToolbarCommands =
        TextListField(graph, instance, "defaultToolbarCommands");
    profile.visibleToolCommands =
        TextListField(graph, instance, "visibleToolCommands");
    profile.shortcutBindings = ShortcutBindings(graph, instance);
    profile.showMenuBar = BoolField(instance, "showMenuBar", true);
    profile.showStatusBar = BoolField(instance, "showStatusBar", true);
    profile.showMainToolbar = BoolField(instance, "showMainToolbar", true);
    profile.exposesGraphEditor = BoolField(instance, "exposesGraphEditor", true);
    profile.exposesProfiler = BoolField(instance, "exposesProfiler", true);
    profile.exposesConsole = BoolField(instance, "exposesConsole", true);
    profile.exposesAssetBrowser = BoolField(instance, "exposesAssetBrowser", true);
    return profile;
}

[[nodiscard]] UIPersona PersonaFromInstance(
    const SDE::CompiledModelGraph& graph,
    const SDE::CompiledInstance& instance)
{
    UIPersona persona = MakeCustomPersona();
    persona.id = instance.instanceId;
    persona.displayName = TextField(instance, "displayName", persona.id);
    persona.description = TextField(instance, "description");
    persona.tier = PersonaTierFromId(TextField(instance, "tier", "custom"));
    persona.themeId = TextField(instance, "themeId", persona.themeId);
    persona.workspacePresetId =
        TextField(instance, "workspacePresetId", persona.workspacePresetId);
    persona.shortcutMapId =
        TextField(instance, "shortcutMapId", persona.shortcutMapId);
    persona.defaultPanels = TextListField(graph, instance, "defaultPanels");
    persona.defaultToolbarCommands =
        TextListField(graph, instance, "defaultToolbarCommands");
    persona.exposesGraphEditor = BoolField(instance, "exposesGraphEditor", true);
    persona.exposesProfiler = BoolField(instance, "exposesProfiler", true);
    persona.exposesConsole = BoolField(instance, "exposesConsole", true);
    persona.exposesAssetBrowser = BoolField(instance, "exposesAssetBrowser", true);
    persona.showShortcutHints = BoolField(instance, "showShortcutHints", true);
    return persona;
}

#endif

} // namespace

EditorCustomizationCatalog::EditorCustomizationCatalog() = default;
EditorCustomizationCatalog::~EditorCustomizationCatalog() = default;

bool EditorCustomizationCatalog::Init(const std::filesystem::path& workspaceRoot)
{
    m_status = {};
    m_snapshot = {};
    const std::filesystem::path resolvedWorkspace =
        ResolveWorkspaceRoot(workspaceRoot);
    m_status.sourceRoot = CustomizationRoot(resolvedWorkspace);
    m_status.userConfigRoot = UserConfigRoot();

#if SAGA_WITH_SDE
    m_status.sdeAvailable = true;
    if (!std::filesystem::exists(m_status.sourceRoot))
    {
        m_status.loaded = false;
        m_status.message =
            "SDE available; no EditorCustomization project found, using built-ins";
        return true;
    }

    SDE::CompilerFacade compiler;
    SDE::CompileRequest request;
    request.workspaceRoot = m_status.sourceRoot;
    request.outputRoot = m_status.sourceRoot / "artifacts";
    request.domain = "Saga.Editor.Customization";
    request.artifactKind = "EditorCustomizationGraph";

    const SDE::CompilerFacadeResult result = compiler.Validate(request);
    for (const SDE::Diagnostic& diagnostic : result.project.validation.diagnostics)
    {
        m_status.diagnostics.push_back(diagnostic.message);
    }

    if (!SDE::IsUsable(result.project.state))
    {
        m_status.loaded = false;
        m_status.message = "SDE editor customisation failed validation";
        return false;
    }

    const SDE::CompilerFacadeResult compiled = compiler.Compile(request);
    for (const SDE::Diagnostic& diagnostic : compiled.project.validation.diagnostics)
    {
        m_status.diagnostics.push_back(diagnostic.message);
    }

    if (!ValidateArtifactContract(compiled.manifest, m_status.diagnostics))
    {
        m_snapshot = {};
        m_status.loaded = false;
        m_status.message =
            "SDE editor customisation artifact is incompatible; using built-ins";
        return false;
    }

    if (compiled.project.graph)
    {
        for (const SDE::CompiledInstance* instance :
             compiled.project.graph->AllOf("EditorProfile"))
        {
            m_snapshot.projectProfiles.push_back(
                ProfileFromInstance(*compiled.project.graph, *instance));
        }
        for (const SDE::CompiledInstance* instance :
             compiled.project.graph->AllOf("EditorPersona"))
        {
            m_snapshot.projectPersonas.push_back(
                PersonaFromInstance(*compiled.project.graph, *instance));
        }
    }

    if (HasBuiltinProfileConflict(m_snapshot.projectProfiles, m_status.diagnostics))
    {
        m_snapshot = {};
        m_status.loaded = false;
        m_status.message =
            "SDE editor customisation artifact conflicts with built-ins; using built-ins";
        return false;
    }

    m_status.loaded = SDE::IsUsable(compiled.project.state);
    m_status.message = m_status.loaded
        ? "SDE editor customisation validated"
        : "SDE editor customisation failed validation";
    return m_status.loaded;
#else
    m_status.sdeAvailable = false;
    m_status.loaded = false;
    m_status.message =
        "SDE customisation unavailable in this build; using built-in profiles";
    return true;
#endif
}

void EditorCustomizationCatalog::Shutdown()
{
    m_status.loaded = false;
    m_snapshot = {};
}

const EditorCustomizationStatus& EditorCustomizationCatalog::Status() const noexcept
{
    return m_status;
}

const EditorCustomizationSnapshot& EditorCustomizationCatalog::Snapshot() const noexcept
{
    return m_snapshot;
}

} // namespace SagaEditor
