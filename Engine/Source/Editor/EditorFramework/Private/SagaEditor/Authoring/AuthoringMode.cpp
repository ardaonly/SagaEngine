// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Settings/IEditorSettingsStore.h"

#include <algorithm>
#include <cctype>

namespace SagaEditor
{
namespace
{
std::string Normalized(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string ThemeForLegacyPersona(const std::string& value)
{
    const std::string id = Normalized(value);
    if (id == "saga.persona.beginner")
        return "Light";
    if (id == "saga.persona.indie" || id == "saga.persona.custom")
        return "Dark";
    if (id == "saga.persona.pro" || id == "saga.persona.technical")
        return "Midnight";
    return {};
}
}

std::string AuthoringModeForLegacyPreference(const std::string& value)
{
    const std::string id = Normalized(value);
    if (id == "quick-production" || id == "balanced" || id == "full-control")
        return id;
    if (id.find("advanced") != std::string::npos ||
        id.find("studio") != std::string::npos ||
        id.find("technical") != std::string::npos ||
        id.ends_with(".pro") || id == "pro")
        return "full-control";
    if (id.find("node") != std::string::npos ||
        id.find("explorer") != std::string::npos ||
        id.find("standard") != std::string::npos ||
        id.find("builder") != std::string::npos ||
        id.find("custom") != std::string::npos ||
        id.find("indie") != std::string::npos)
        return "balanced";
    if (id.find("basic") != std::string::npos ||
        id.find("creator") != std::string::npos ||
        id.find("beginner") != std::string::npos)
        return "quick-production";
    return {};
}

std::string CanonicalEditorWorkModeId(const std::string& value)
{
    const std::string id = Normalized(value);
    if (id == "level1") return "quick-production";
    if (id == "level2") return "balanced";
    if (id == "level3") return "full-control";
    return AuthoringModeForLegacyPreference(id);
}

void MigrateLegacyEditorPreferences(IEditorSettingsStore& settings)
{
    if (settings.GetString(EditorWorkModeService::MigrationVersionKey, {}) == "3")
        return;
    std::string mode = CanonicalEditorWorkModeId(
        settings.GetString(EditorWorkModeService::SettingKey, {}));
    if (mode.empty())
    {
        mode = CanonicalEditorWorkModeId(settings.GetString(
            "Editor/AuthoringMode/v1", {}));
    }
    if (mode.empty())
    {
        mode = AuthoringModeForLegacyPreference(
            settings.GetString("profile.activeId", {}));
        if (mode.empty())
            mode = AuthoringModeForLegacyPreference(
                settings.GetString("persona.activeId", {}));
    }
    if (!mode.empty())
        settings.SetString(EditorWorkModeService::SettingKey, mode);
    if (settings.GetString(EditorWorkModeService::AppearanceSettingKey, {}).empty())
    {
        const std::string theme = ThemeForLegacyPersona(
            settings.GetString("persona.activeId", {}));
        if (!theme.empty())
            settings.SetString(EditorWorkModeService::AppearanceSettingKey, theme);
    }
    for (const char* key : {"profile.activeId", "persona.activeId",
             "initialProfileId", "user.workspace.default",
             "Editor/AuthoringMode/v1", "Editor/Layout/v2/level1",
             "Editor/Layout/v2/level2", "Editor/Layout/v2/level3"})
        settings.Remove(key);
    settings.SetString(EditorWorkModeService::MigrationVersionKey, "3");
    settings.Flush();
}

bool EditorWorkModeService::Init(IEditorSettingsStore& settings,
                                const std::string& explicitMode,
                                const std::string& legacyProfile)
{
    modes_ = {
        {"quick-production", "Quick Production",
            {"saga.panel.hierarchy", "saga.panel.viewport", "saga.panel.inspector"},
            {"saga.command.world.play", "saga.command.file.save"},
            "comfortable", "essential", "guided"},
        {"balanced", "Balanced",
            {"saga.panel.hierarchy", "saga.panel.viewport",
             "saga.panel.inspector", "saga.panel.code_box"},
            {"saga.command.world.play", "saga.command.file.save",
             "saga.command.view.code_box"},
            "standard", "standard", "flexible"},
        {"full-control", "Full Control",
            {"saga.panel.hierarchy", "saga.panel.viewport",
            "saga.panel.inspector", "saga.panel.code_box", "saga.panel.assetbrowser",
            "saga.panel.problems", "saga.panel.packages",
            "saga.panel.performance_transition"},
            {"saga.command.world.play", "saga.command.file.save",
             "saga.command.view.code_box", "saga.command.view.problems",
             "saga.command.view.packages", "saga.command.view.performance_transition"},
            "compact", "verbose", "advanced"},
    };
    settings_ = &settings;
    MigrateLegacyEditorPreferences(settings);
    std::string selected = CanonicalEditorWorkModeId(explicitMode);
    if (!explicitMode.empty() && selected.empty()) return false;
    if (selected.empty() && !legacyProfile.empty())
        selected = AuthoringModeForLegacyPreference(legacyProfile);
    if (selected.empty())
        selected = CanonicalEditorWorkModeId(settings.GetString(SettingKey, {}));
    if (selected.empty())
        selected = "quick-production";
    if (!IsValid(selected))
        return explicitMode.empty() ? SetActive("quick-production") : false;
    return SetActive(selected);
}

bool EditorWorkModeService::SetActive(const std::string& requestedId)
{
    const std::string id = CanonicalEditorWorkModeId(requestedId);
    const auto it = std::find_if(modes_.begin(), modes_.end(),
        [&id](const EditorWorkMode& mode) { return mode.id == id; });
    if (it == modes_.end())
        return false;
    active_ = static_cast<std::size_t>(std::distance(modes_.begin(), it));
    if (settings_)
    {
        settings_->SetString(SettingKey, id);
        settings_->Flush();
    }
    if (changed_)
        changed_(*it);
    return true;
}

const EditorWorkMode& EditorWorkModeService::Active() const noexcept { return modes_[active_]; }
const std::vector<EditorWorkMode>& EditorWorkModeService::Modes() const noexcept { return modes_; }
bool EditorWorkModeService::IsValid(const std::string& requestedId) const noexcept
{
    const std::string id = CanonicalEditorWorkModeId(requestedId);
    return std::any_of(modes_.begin(), modes_.end(),
        [&id](const EditorWorkMode& mode) { return mode.id == id; });
}
void EditorWorkModeService::SetOnChanged(Changed changed) { changed_ = std::move(changed); }
} // namespace SagaEditor
