// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace SagaEditor
{
class IEditorSettingsStore;

struct EditorWorkMode
{
    std::string id;
    std::string displayName;
    std::vector<std::string> defaultVisiblePanels;
    std::vector<std::string> defaultCommands;
    std::string densityPolicy;
    std::string diagnosticPolicy;
    std::string dockingPolicy;
};

class EditorWorkModeService
{
public:
    using Changed = std::function<void(const EditorWorkMode&)>;

    [[nodiscard]] bool Init(IEditorSettingsStore& settings,
                            const std::string& explicitMode = {},
                            const std::string& legacyProfile = {});
    [[nodiscard]] bool SetActive(const std::string& id);
    [[nodiscard]] const EditorWorkMode& Active() const noexcept;
    [[nodiscard]] const std::vector<EditorWorkMode>& Modes() const noexcept;
    [[nodiscard]] bool IsValid(const std::string& id) const noexcept;
    void SetOnChanged(Changed changed);

    static constexpr const char* SettingKey = "Editor/WorkMode/v1";
    static constexpr const char* AppearanceSettingKey = "Editor/Appearance/v1";
    static constexpr const char* ThemeSettingKey = AppearanceSettingKey;
    static constexpr const char* MigrationVersionKey =
        "Editor/PreferenceMigration/version";

private:
    IEditorSettingsStore* settings_ = nullptr;
    std::vector<EditorWorkMode> modes_;
    std::size_t active_ = 0;
    Changed changed_;
};

using AuthoringMode = EditorWorkMode;
using AuthoringModeService = EditorWorkModeService;

/// One-time, idempotent conversion of private legacy preferences.
void MigrateLegacyEditorPreferences(IEditorSettingsStore& settings);
[[nodiscard]] std::string AuthoringModeForLegacyPreference(
    const std::string& value);
[[nodiscard]] std::string CanonicalEditorWorkModeId(const std::string& value);
} // namespace SagaEditor
