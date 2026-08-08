// SPDX-License-Identifier: MPL-2.0
/// @file UserWorkspace.h
/// @brief Qt-free official work-mode and personal workspace persistence.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
class IEditorSettingsStore;
struct EditorWorkMode;

struct WorkspaceDockDefinition
{
    std::string panelId;
    std::string area;
    bool visible = true;
    bool floating = false;
};

struct WorkspaceDefinition
{
    std::string id;
    std::string displayName;
    std::string modeId;
    std::vector<WorkspaceDockDefinition> docks;
    std::vector<double> splitters;
    std::string activeAuthoringSurface;
    std::string machinePlacementState;
};

struct UserWorkspaceOverlay
{
    std::string workspaceId;
    std::vector<WorkspaceDockDefinition> dockOverrides;
    std::string activeAuthoringSurface;
    std::string machinePlacementState;
};

struct AppearanceSettings
{
    std::string theme = "Dark";
    std::string density = "system";
    std::string accent = "default";
};

struct EffectiveWorkspacePresentation
{
    std::string identity;
    std::string modeId;
    std::vector<WorkspaceDockDefinition> docks;
    std::vector<double> splitters;
    std::string activeAuthoringSurface;
    std::string machinePlacementState;
    AppearanceSettings appearance;
};

struct UserWorkspaceState
{
    std::vector<WorkspaceDefinition> namedWorkspaces;
    std::string currentWorkspaceId;
    bool dirty = false;
    bool recoveredFromCorruption = false;
    std::string diagnostic;
};

class UserWorkspaceService
{
public:
    static constexpr std::uint32_t SchemaVersion = 1;
    static constexpr std::uintmax_t MaximumFileBytes = 1024 * 1024;
    static constexpr const char* SelectionSettingKey =
        "Editor/WorkspaceSelection/v1";
    static constexpr const char* AppearanceSettingKey =
        "Editor/Appearance/v1";
    static constexpr const char* AppearanceDensitySettingKey =
        "Editor/AppearanceDensity/v1";
    static constexpr const char* AppearanceAccentSettingKey =
        "Editor/AppearanceAccent/v1";

    [[nodiscard]] bool Init(IEditorSettingsStore& settings,
        std::filesystem::path configurationRoot,
        const std::vector<EditorWorkMode>& modes);
    [[nodiscard]] bool ActivateMode(const EditorWorkMode& mode);
    [[nodiscard]] bool CaptureUserOverlay(std::vector<std::uint8_t> state);
    [[nodiscard]] bool SetPanelVisible(const std::string& panelId, bool visible);
    [[nodiscard]] bool SaveCurrent();
    [[nodiscard]] bool SaveAs(const std::string& displayName,
        std::vector<std::uint8_t> state);
    [[nodiscard]] bool SaveAs(const std::string& displayName);
    [[nodiscard]] bool Load(const std::string& id);
    [[nodiscard]] bool Rename(const std::string& id,
        const std::string& displayName);
    [[nodiscard]] bool Delete(const std::string& id);
    [[nodiscard]] bool ResetCurrentWorkspace();
    [[nodiscard]] bool ResetModeDefaults();
    [[nodiscard]] bool SetAppearance(AppearanceSettings appearance);

    [[nodiscard]] const UserWorkspaceState& State() const noexcept;
    [[nodiscard]] EffectiveWorkspacePresentation Effective() const;
    [[nodiscard]] const AppearanceSettings& Appearance() const noexcept;
    [[nodiscard]] const std::filesystem::path& StoragePath() const noexcept;

private:
    [[nodiscard]] bool Read();
    [[nodiscard]] bool Write();
    [[nodiscard]] bool SelectModeDefault();
    [[nodiscard]] const EditorWorkMode* FindMode(const std::string& id) const;
    [[nodiscard]] WorkspaceDefinition ModeDefault(const EditorWorkMode& mode) const;
    [[nodiscard]] WorkspaceDefinition* FindWorkspace(const std::string& id);
    [[nodiscard]] const WorkspaceDefinition* FindWorkspace(const std::string& id) const;
    [[nodiscard]] std::string CurrentIdentity() const;
    [[nodiscard]] UserWorkspaceOverlay* FindOverlay(const std::string& id);
    [[nodiscard]] const UserWorkspaceOverlay* FindOverlay(
        const std::string& id) const;
    void RemoveOverlay(const std::string& id);
    void QuarantineCorruptFile(const std::string& diagnostic);

    IEditorSettingsStore* settings_ = nullptr;
    std::filesystem::path storagePath_;
    std::vector<EditorWorkMode> modes_;
    std::string activeModeId_;
    UserWorkspaceState state_;
    std::vector<UserWorkspaceOverlay> overlays_;
    AppearanceSettings appearance_;
};
} // namespace SagaEditor
