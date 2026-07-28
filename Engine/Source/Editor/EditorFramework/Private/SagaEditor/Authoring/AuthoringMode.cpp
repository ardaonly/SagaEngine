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
    if (id.find("advanced") != std::string::npos ||
        id.find("studio") != std::string::npos ||
        id.find("technical") != std::string::npos ||
        id.ends_with(".pro") || id == "pro")
        return "level3";
    if (id.find("node") != std::string::npos ||
        id.find("explorer") != std::string::npos ||
        id.find("standard") != std::string::npos ||
        id.find("builder") != std::string::npos ||
        id.find("custom") != std::string::npos ||
        id.find("indie") != std::string::npos)
        return "level2";
    if (id.find("basic") != std::string::npos ||
        id.find("creator") != std::string::npos ||
        id.find("beginner") != std::string::npos)
        return "level1";
    return {};
}

void MigrateLegacyEditorPreferences(IEditorSettingsStore& settings)
{
    if (settings.GetString(AuthoringModeService::MigrationVersionKey, {}) == "2")
        return;
    if (settings.GetString(AuthoringModeService::SettingKey, {}).empty())
    {
        // Profile wins because it was the final authority for production layout.
        std::string mode = AuthoringModeForLegacyPreference(
            settings.GetString("profile.activeId", {}));
        if (mode.empty())
            mode = AuthoringModeForLegacyPreference(
                settings.GetString("persona.activeId", {}));
        if (!mode.empty())
            settings.SetString(AuthoringModeService::SettingKey, mode);
    }
    if (settings.GetString(AuthoringModeService::ThemeSettingKey, {}).empty())
    {
        const std::string theme = ThemeForLegacyPersona(
            settings.GetString("persona.activeId", {}));
        if (!theme.empty())
            settings.SetString(AuthoringModeService::ThemeSettingKey, theme);
    }
    settings.SetString(AuthoringModeService::MigrationVersionKey, "2");
    settings.Flush();
}

bool AuthoringModeService::Init(IEditorSettingsStore& settings,
                                const std::string& explicitMode,
                                const std::string& legacyProfile)
{
    modes_ = {
        {"level1", "Level 1", {"saga.panel.hierarchy", "saga.panel.viewport",
            "saga.panel.inspector"}},
        {"level2", "Level 2", {"saga.panel.hierarchy", "saga.panel.viewport",
            "saga.panel.inspector", "saga.panel.code_box"}},
        {"level3", "Level 3", {"saga.panel.hierarchy", "saga.panel.viewport",
            "saga.panel.inspector", "saga.panel.code_box", "saga.panel.assetbrowser",
            "saga.panel.problems"}},
    };
    settings_ = &settings;
    MigrateLegacyEditorPreferences(settings);
    std::string selected = explicitMode;
    if (selected.empty() && !legacyProfile.empty())
        selected = AuthoringModeForLegacyPreference(legacyProfile);
    if (selected.empty())
        selected = settings.GetString(SettingKey, {});
    if (selected.empty())
        selected = "level1";
    if (!IsValid(selected))
        return explicitMode.empty() ? SetActive("level1") : false;
    return SetActive(selected);
}

bool AuthoringModeService::SetActive(const std::string& id)
{
    const auto it = std::find_if(modes_.begin(), modes_.end(),
        [&id](const AuthoringMode& mode) { return mode.id == id; });
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

const AuthoringMode& AuthoringModeService::Active() const noexcept { return modes_[active_]; }
const std::vector<AuthoringMode>& AuthoringModeService::Modes() const noexcept { return modes_; }
bool AuthoringModeService::IsValid(const std::string& id) const noexcept
{
    return std::any_of(modes_.begin(), modes_.end(),
        [&id](const AuthoringMode& mode) { return mode.id == id; });
}
void AuthoringModeService::SetOnChanged(Changed changed) { changed_ = std::move(changed); }
} // namespace SagaEditor
