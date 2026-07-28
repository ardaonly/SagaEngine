// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace SagaEditor
{
class IEditorSettingsStore;

struct AuthoringMode
{
    std::string id;
    std::string displayName;
    std::vector<std::string> defaultVisiblePanels;
};

class AuthoringModeService
{
public:
    using Changed = std::function<void(const AuthoringMode&)>;

    [[nodiscard]] bool Init(IEditorSettingsStore& settings,
                            const std::string& explicitMode = {},
                            const std::string& legacyProfile = {});
    [[nodiscard]] bool SetActive(const std::string& id);
    [[nodiscard]] const AuthoringMode& Active() const noexcept;
    [[nodiscard]] const std::vector<AuthoringMode>& Modes() const noexcept;
    [[nodiscard]] bool IsValid(const std::string& id) const noexcept;
    void SetOnChanged(Changed changed);

    static constexpr const char* SettingKey = "Editor/AuthoringMode/v1";
    static constexpr const char* ThemeSettingKey = "Editor/Theme/v1";
    static constexpr const char* MigrationVersionKey =
        "Editor/PreferenceMigration/version";

private:
    IEditorSettingsStore* settings_ = nullptr;
    std::vector<AuthoringMode> modes_;
    std::size_t active_ = 0;
    Changed changed_;
};

/// One-time, idempotent conversion of private legacy preferences.
void MigrateLegacyEditorPreferences(IEditorSettingsStore& settings);
[[nodiscard]] std::string AuthoringModeForLegacyPreference(
    const std::string& value);
} // namespace SagaEditor
