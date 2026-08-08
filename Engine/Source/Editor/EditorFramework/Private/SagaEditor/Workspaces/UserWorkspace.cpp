// SPDX-License-Identifier: MPL-2.0
/// @file UserWorkspace.cpp
/// @brief Versioned personal workspace store and precedence resolver.

#include "SagaEditor/Workspaces/UserWorkspace.h"

#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Settings/IEditorSettingsStore.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;

std::string EncodeHex(const std::vector<std::uint8_t>& bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(bytes.size() * 2, '0');
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        result[index * 2] = digits[bytes[index] >> 4U];
        result[index * 2 + 1] = digits[bytes[index] & 0x0fU];
    }
    return result;
}

bool IsBoundedText(const std::string& value, std::size_t maximum = 128)
{
    return !value.empty() && value.size() <= maximum &&
        std::all_of(value.begin(), value.end(), [](unsigned char value)
        { return value >= 0x20 && value < 0x7f; });
}

bool IsHexState(const std::string& value)
{
    return value.size() <= UserWorkspaceService::MaximumFileBytes &&
        value.size() % 2 == 0 &&
        std::all_of(value.begin(), value.end(), [](unsigned char character)
        { return std::isxdigit(character) != 0; });
}

std::string WorkspaceId(const std::string& displayName)
{
    std::string id;
    bool separator = false;
    for (const unsigned char value : displayName)
    {
        if (std::isalnum(value))
        {
            if (separator && !id.empty()) id.push_back('-');
            id.push_back(static_cast<char>(std::tolower(value)));
            separator = false;
        }
        else separator = true;
        if (id.size() == 48) break;
    }
    return id;
}

WorkspaceDockDefinition DockFromJson(const Json& value)
{
    WorkspaceDockDefinition dock;
    dock.panelId = value.value("panelId", "");
    dock.area = value.value("area", "");
    dock.visible = value.value("visible", true);
    dock.floating = value.value("floating", false);
    if (!IsBoundedText(dock.panelId) || !IsBoundedText(dock.area, 32))
        throw std::runtime_error("Workspace dock identity is invalid.");
    if (dock.area != "left" && dock.area != "right" &&
        dock.area != "bottom" && dock.area != "center")
        throw std::runtime_error("Workspace dock area is invalid.");
    return dock;
}

Json DockToJson(const WorkspaceDockDefinition& dock)
{
    return {{"panelId", dock.panelId}, {"area", dock.area},
        {"visible", dock.visible}, {"floating", dock.floating}};
}

std::vector<WorkspaceDockDefinition> DocksFromJson(const Json& values)
{
    if (!values.is_array() || values.size() > 64)
        throw std::runtime_error("Workspace dock list is invalid.");
    std::vector<WorkspaceDockDefinition> result;
    for (const auto& value : values)
    {
        auto dock = DockFromJson(value);
        if (std::any_of(result.begin(), result.end(),
            [&dock](const WorkspaceDockDefinition& existing)
            { return existing.panelId == dock.panelId; }))
            throw std::runtime_error("Workspace dock identity is duplicated.");
        result.push_back(std::move(dock));
    }
    return result;
}

Json DocksToJson(const std::vector<WorkspaceDockDefinition>& values)
{
    Json result = Json::array();
    for (const auto& value : values) result.push_back(DockToJson(value));
    return result;
}

std::string DefaultArea(const std::string& panelId)
{
    if (panelId == "saga.panel.hierarchy") return "left";
    if (panelId == "saga.panel.inspector") return "right";
    if (panelId == "saga.panel.viewport") return "center";
    return "bottom";
}

const std::vector<std::string>& ProductPanelIds()
{
    static const std::vector<std::string> ids = {
        "saga.panel.hierarchy", "saga.panel.viewport", "saga.panel.inspector",
        "saga.panel.assetbrowser", "saga.panel.code_box", "saga.panel.problems",
        "saga.panel.packages", "saga.panel.performance_transition",
        "saga.panel.customize_workspace"};
    return ids;
}
} // namespace

bool UserWorkspaceService::Init(IEditorSettingsStore& settings,
    std::filesystem::path configurationRoot,
    const std::vector<EditorWorkMode>& modes)
{
    settings_ = &settings;
    modes_ = modes;
    if (modes_.empty()) return false;
    activeModeId_ = modes_.front().id;
    const auto selectedMode = CanonicalEditorWorkModeId(
        settings.GetString(EditorWorkModeService::SettingKey, {}));
    if (FindMode(selectedMode)) activeModeId_ = selectedMode;
    if (!configurationRoot.empty())
        storagePath_ = std::move(configurationRoot) / "workspaces-v1.json";
    appearance_.theme = settings.GetString(AppearanceSettingKey, "Dark");
    appearance_.density = settings.GetString(
        AppearanceDensitySettingKey, "system");
    appearance_.accent = settings.GetString(
        AppearanceAccentSettingKey, "default");
    if (!Read()) return false;
    if (!MigrateLegacyWorkspaces()) return true;
    state_.currentWorkspaceId = settings.GetString(SelectionSettingKey, {});
    if (!state_.currentWorkspaceId.empty() &&
        !FindWorkspace(state_.currentWorkspaceId))
        (void)SelectModeDefault();
    return true;
}

bool UserWorkspaceService::MigrateLegacyWorkspaces()
{
    if (!settings_ || settings_->GetString(MigrationVersionKey, {}) == "1") return true;
    struct LegacyLayout { const char* key; const char* mode; };
    constexpr LegacyLayout layouts[] = {
        {"Editor/Layout/v2/level1", "quick-production"},
        {"Editor/Layout/v2/level2", "balanced"},
        {"Editor/Layout/v2/level3", "full-control"},
    };
    std::vector<std::string> importedKeys;
    bool changed = false;
    for (const auto& layout : layouts)
    {
        const auto bytes = settings_->GetBytes(layout.key);
        if (!bytes) continue;
        if (bytes->empty() || bytes->size() * 2 > MaximumFileBytes)
        {
            state_.diagnostic = "Workspace.MigrationPending: invalid legacy layout retained: " +
                std::string(layout.key);
            continue;
        }
        const std::string id = "mode:" + std::string(layout.mode);
        auto* overlay = FindOverlay(id);
        if (!overlay) { overlays_.push_back({id}); overlay = &overlays_.back(); }
        overlay->machinePlacementState = EncodeHex(*bytes);
        importedKeys.emplace_back(layout.key);
        changed = true;
    }
    if (const auto bytes = settings_->GetBytes("user.workspace.default"))
    {
        if (bytes->empty() || bytes->size() * 2 > MaximumFileBytes)
            state_.diagnostic =
                "Workspace.MigrationPending: invalid legacy default workspace retained.";
        else
        {
            auto* existing = FindWorkspace("legacy-default");
            if (!existing)
            {
                auto definition = ModeDefault(*FindMode(activeModeId_));
                definition.id = "legacy-default";
                definition.displayName = "Legacy Default";
                definition.machinePlacementState = EncodeHex(*bytes);
                state_.namedWorkspaces.push_back(std::move(definition));
            }
            else existing->machinePlacementState = EncodeHex(*bytes);
            importedKeys.emplace_back("user.workspace.default");
            changed = true;
        }
    }
    if (!changed)
    {
        if (state_.diagnostic.empty())
        {
            settings_->SetString(MigrationVersionKey, "1");
            settings_->Flush();
        }
        return true;
    }
    if (!Write())
    {
        state_.diagnostic =
            "Workspace.MigrationPending: canonical workspace write failed; legacy keys retained.";
        return false;
    }
    // Re-read the atomic output before deleting any source key. Stable ids make
    // re-entry after a write/delete interruption replace rather than duplicate.
    if (!Read()) return false;
    for (const auto& key : importedKeys) settings_->Remove(key);
    if (state_.diagnostic.empty()) settings_->SetString(MigrationVersionKey, "1");
    settings_->Flush();
    return true;
}

bool UserWorkspaceService::ActivateMode(const EditorWorkMode& mode)
{
    if (!FindMode(mode.id)) return false;
    activeModeId_ = mode.id;
    const auto* selected = FindWorkspace(state_.currentWorkspaceId);
    if (!selected || selected->modeId != activeModeId_)
        return SelectModeDefault();
    return true;
}

bool UserWorkspaceService::CaptureUserOverlay(std::vector<std::uint8_t> state)
{
    if (activeModeId_.empty()) return false;
    const std::string identity = CurrentIdentity();
    auto* overlay = FindOverlay(identity);
    if (!overlay)
    {
        overlays_.push_back({});
        overlay = &overlays_.back();
        overlay->workspaceId = identity;
    }
    overlay->machinePlacementState = EncodeHex(state);
    state_.dirty = true;
    return true;
}

bool UserWorkspaceService::SetPanelVisible(const std::string& panelId, bool visible)
{
    if (std::find(ProductPanelIds().begin(), ProductPanelIds().end(), panelId) ==
        ProductPanelIds().end()) return false;
    const std::string identity = CurrentIdentity();
    auto* overlay = FindOverlay(identity);
    if (!overlay)
    {
        overlays_.push_back({});
        overlay = &overlays_.back();
        overlay->workspaceId = identity;
    }
    auto found = std::find_if(overlay->dockOverrides.begin(),
        overlay->dockOverrides.end(), [&panelId](const WorkspaceDockDefinition& dock)
        { return dock.panelId == panelId; });
    if (found == overlay->dockOverrides.end())
        overlay->dockOverrides.push_back(
            {panelId, DefaultArea(panelId), visible, false});
    else found->visible = visible;
    state_.dirty = true;
    return true;
}

bool UserWorkspaceService::SaveCurrent()
{
    if (!state_.dirty) return true;
    const bool written = Write();
    if (written) state_.dirty = false;
    return written;
}

bool UserWorkspaceService::SaveAs(const std::string& displayName,
    std::vector<std::uint8_t> state)
{
    if (!IsBoundedText(displayName)) return false;
    std::string id = WorkspaceId(displayName);
    if (id.empty() || FindWorkspace(id)) return false;
    WorkspaceDefinition definition = ModeDefault(*FindMode(activeModeId_));
    definition.id = id;
    definition.displayName = displayName;
    definition.machinePlacementState = EncodeHex(state);
    const UserWorkspaceState previous = state_;
    state_.namedWorkspaces.push_back(std::move(definition));
    state_.currentWorkspaceId = id;
    if (!Write())
    {
        state_ = previous;
        return false;
    }
    settings_->SetString(SelectionSettingKey, id);
    settings_->Flush();
    state_.dirty = false;
    return true;
}

bool UserWorkspaceService::SaveAs(const std::string& displayName)
{
    const auto* captured = FindOverlay(CurrentIdentity());
    if (!captured || captured->machinePlacementState.empty()) return false;
    if (!IsBoundedText(displayName)) return false;
    std::string id = WorkspaceId(displayName);
    if (id.empty() || FindWorkspace(id)) return false;
    WorkspaceDefinition definition = ModeDefault(*FindMode(activeModeId_));
    definition.id = id;
    definition.displayName = displayName;
    definition.machinePlacementState = captured->machinePlacementState;
    const UserWorkspaceState previous = state_;
    state_.namedWorkspaces.push_back(std::move(definition));
    state_.currentWorkspaceId = id;
    if (!Write())
    {
        state_ = previous;
        return false;
    }
    settings_->SetString(SelectionSettingKey, id);
    settings_->Flush();
    state_.dirty = false;
    return true;
}

bool UserWorkspaceService::Load(const std::string& id)
{
    const auto* workspace = FindWorkspace(id);
    if (!workspace || workspace->modeId != activeModeId_) return false;
    state_.currentWorkspaceId = id;
    state_.dirty = false;
    settings_->SetString(SelectionSettingKey, id);
    settings_->Flush();
    return true;
}

bool UserWorkspaceService::Rename(const std::string& id,
    const std::string& displayName)
{
    auto* workspace = FindWorkspace(id);
    if (!workspace || !IsBoundedText(displayName)) return false;
    const std::string previous = workspace->displayName;
    workspace->displayName = displayName;
    if (Write()) return true;
    workspace->displayName = previous;
    return false;
}

bool UserWorkspaceService::Delete(const std::string& id)
{
    const auto found = std::find_if(state_.namedWorkspaces.begin(),
        state_.namedWorkspaces.end(), [&id](const WorkspaceDefinition& value)
        { return value.id == id; });
    if (found == state_.namedWorkspaces.end()) return false;
    const UserWorkspaceState previousState = state_;
    const auto previousOverlays = overlays_;
    const bool active = state_.currentWorkspaceId == id;
    RemoveOverlay(id);
    state_.namedWorkspaces.erase(found);
    if (active) state_.currentWorkspaceId.clear();
    if (!Write())
    {
        state_ = previousState;
        overlays_ = previousOverlays;
        return false;
    }
    if (active)
    {
        settings_->Remove(SelectionSettingKey);
        settings_->Flush();
    }
    return true;
}

bool UserWorkspaceService::ResetCurrentWorkspace()
{
    const auto previousOverlays = overlays_;
    const std::string previousSelection = state_.currentWorkspaceId;
    RemoveOverlay(CurrentIdentity());
    state_.currentWorkspaceId.clear();
    if (!Write())
    {
        overlays_ = previousOverlays;
        state_.currentWorkspaceId = previousSelection;
        return false;
    }
    settings_->Remove(SelectionSettingKey);
    settings_->Flush();
    state_.dirty = false;
    return true;
}

bool UserWorkspaceService::ResetModeDefaults()
{
    const auto previousOverlays = overlays_;
    const std::string previousSelection = state_.currentWorkspaceId;
    RemoveOverlay("mode:" + activeModeId_);
    state_.currentWorkspaceId.clear();
    if (!Write())
    {
        overlays_ = previousOverlays;
        state_.currentWorkspaceId = previousSelection;
        return false;
    }
    settings_->Remove(SelectionSettingKey);
    settings_->Flush();
    state_.dirty = false;
    return true;
}

bool UserWorkspaceService::SetAppearance(AppearanceSettings appearance)
{
    if (!IsBoundedText(appearance.theme, 64) ||
        !IsBoundedText(appearance.density, 32) ||
        !IsBoundedText(appearance.accent, 32)) return false;
    appearance_ = std::move(appearance);
    settings_->SetString(AppearanceSettingKey, appearance_.theme);
    settings_->SetString(AppearanceDensitySettingKey, appearance_.density);
    settings_->SetString(AppearanceAccentSettingKey, appearance_.accent);
    settings_->Flush();
    return true;
}

const UserWorkspaceState& UserWorkspaceService::State() const noexcept
{ return state_; }
const AppearanceSettings& UserWorkspaceService::Appearance() const noexcept
{ return appearance_; }
const std::filesystem::path& UserWorkspaceService::StoragePath() const noexcept
{ return storagePath_; }

EffectiveWorkspacePresentation UserWorkspaceService::Effective() const
{
    const EditorWorkMode* mode = FindMode(activeModeId_);
    WorkspaceDefinition definition = ModeDefault(*mode);
    if (const auto* selected = FindWorkspace(state_.currentWorkspaceId))
        definition = *selected;
    EffectiveWorkspacePresentation result;
    result.identity = definition.id;
    result.modeId = activeModeId_;
    result.docks = definition.docks;
    result.splitters = definition.splitters;
    result.activeAuthoringSurface = definition.activeAuthoringSurface;
    const auto* overlay = FindOverlay(CurrentIdentity());
    result.machinePlacementState = !overlay || overlay->machinePlacementState.empty() ?
        definition.machinePlacementState : overlay->machinePlacementState;
    std::unordered_map<std::string, WorkspaceDockDefinition*> docks;
    for (auto& dock : result.docks) docks.emplace(dock.panelId, &dock);
    if (overlay)
    {
        for (const auto& override : overlay->dockOverrides)
            if (const auto found = docks.find(override.panelId); found != docks.end())
                *found->second = override;
        if (!overlay->activeAuthoringSurface.empty())
            result.activeAuthoringSurface = overlay->activeAuthoringSurface;
    }
    result.appearance = appearance_;
    return result;
}

bool UserWorkspaceService::Read()
{
    state_ = {};
    overlays_.clear();
    if (storagePath_.empty() || !std::filesystem::exists(storagePath_)) return true;
    std::error_code error;
    if (std::filesystem::file_size(storagePath_, error) > MaximumFileBytes || error)
    {
        QuarantineCorruptFile("Workspace file exceeds the supported size.");
        return true;
    }
    try
    {
        std::ifstream input(storagePath_);
        const Json root = Json::parse(input);
        if (root.value("schemaVersion", 0U) != SchemaVersion)
            throw std::runtime_error("Workspace schema version is unsupported.");
        const auto& values = root.at("workspaces");
        if (!values.is_array() || values.size() > 128)
            throw std::runtime_error("Workspace collection is invalid.");
        for (const auto& value : values)
        {
            WorkspaceDefinition definition;
            definition.id = value.value("id", "");
            definition.displayName = value.value("displayName", "");
            definition.modeId = CanonicalEditorWorkModeId(value.value("modeId", ""));
            definition.docks = DocksFromJson(value.value("docks", Json::array()));
            definition.splitters = value.value("splitters", std::vector<double>{});
            definition.activeAuthoringSurface =
                value.value("activeAuthoringSurface", "");
            definition.machinePlacementState =
                value.value("machinePlacementState", "");
            if (!IsBoundedText(definition.id, 64) ||
                !IsBoundedText(definition.displayName) ||
                !FindMode(definition.modeId) || definition.splitters.size() > 32 ||
                !IsHexState(definition.machinePlacementState) ||
                !std::all_of(definition.splitters.begin(),
                    definition.splitters.end(), [](double value)
                    { return value >= 0.0 && value <= 1.0; }))
                throw std::runtime_error("Workspace definition is invalid.");
            if (FindWorkspace(definition.id))
                throw std::runtime_error("Workspace identity is duplicated.");
            state_.namedWorkspaces.push_back(std::move(definition));
        }
        const auto overlayValues = root.value("overlays", Json::array());
        if (!overlayValues.is_array() || overlayValues.size() > 128)
            throw std::runtime_error("Workspace overlay collection is invalid.");
        for (const auto& value : overlayValues)
        {
            UserWorkspaceOverlay overlay;
            overlay.workspaceId = value.value("workspaceId", "");
            overlay.dockOverrides = DocksFromJson(
                value.value("dockOverrides", Json::array()));
            overlay.activeAuthoringSurface =
                value.value("activeAuthoringSurface", "");
            overlay.machinePlacementState =
                value.value("machinePlacementState", "");
            if (!IsBoundedText(overlay.workspaceId, 64) ||
                !IsHexState(overlay.machinePlacementState) ||
                FindOverlay(overlay.workspaceId))
                throw std::runtime_error("Workspace overlay is invalid.");
            overlays_.push_back(std::move(overlay));
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        QuarantineCorruptFile(exception.what());
        return true;
    }
}

bool UserWorkspaceService::Write()
{
    if (storagePath_.empty()) return true;
    Json root{{"schemaVersion", SchemaVersion}, {"workspaces", Json::array()},
        {"overlays", Json::array()}};
    for (const auto& definition : state_.namedWorkspaces)
    {
        root["workspaces"].push_back({
            {"id", definition.id}, {"displayName", definition.displayName},
            {"modeId", definition.modeId}, {"docks", DocksToJson(definition.docks)},
            {"splitters", definition.splitters},
            {"activeAuthoringSurface", definition.activeAuthoringSurface},
            {"machinePlacementState", definition.machinePlacementState}});
    }
    for (const auto& overlay : overlays_)
        root["overlays"].push_back({{"workspaceId", overlay.workspaceId},
            {"dockOverrides", DocksToJson(overlay.dockOverrides)},
            {"activeAuthoringSurface", overlay.activeAuthoringSurface},
            {"machinePlacementState", overlay.machinePlacementState}});
    std::error_code error;
    std::filesystem::create_directories(storagePath_.parent_path(), error);
    if (error) return false;
    const auto temporary = storagePath_.string() + ".tmp";
    const auto backup = storagePath_.string() + ".previous";
    {
        std::ofstream output(temporary, std::ios::trunc);
        output << root.dump(2) << '\n';
        if (!output)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
    }
    std::filesystem::remove(backup, error);
    error.clear();
    if (std::filesystem::exists(storagePath_))
    {
        std::filesystem::rename(storagePath_, backup, error);
        if (error)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }
    std::filesystem::rename(temporary, storagePath_, error);
    if (error)
    {
        std::error_code ignored;
        if (std::filesystem::exists(backup))
            std::filesystem::rename(backup, storagePath_, ignored);
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    std::filesystem::remove(backup, error);
    return true;
}

bool UserWorkspaceService::SelectModeDefault()
{
    state_.currentWorkspaceId.clear();
    state_.dirty = false;
    if (settings_)
    {
        settings_->Remove(SelectionSettingKey);
        settings_->Flush();
    }
    return true;
}

const EditorWorkMode* UserWorkspaceService::FindMode(const std::string& id) const
{
    const auto found = std::find_if(modes_.begin(), modes_.end(),
        [&id](const EditorWorkMode& value) { return value.id == id; });
    return found == modes_.end() ? nullptr : &*found;
}

WorkspaceDefinition UserWorkspaceService::ModeDefault(
    const EditorWorkMode& mode) const
{
    WorkspaceDefinition result;
    result.id = "mode:" + mode.id;
    result.displayName = mode.displayName + " Defaults";
    result.modeId = mode.id;
    result.activeAuthoringSurface = "saga.panel.viewport";
    for (const auto& panel : ProductPanelIds())
        result.docks.push_back({panel, DefaultArea(panel),
            std::find(mode.defaultVisiblePanels.begin(),
                mode.defaultVisiblePanels.end(), panel) !=
                mode.defaultVisiblePanels.end(), false});
    return result;
}

WorkspaceDefinition* UserWorkspaceService::FindWorkspace(const std::string& id)
{
    const auto found = std::find_if(state_.namedWorkspaces.begin(),
        state_.namedWorkspaces.end(), [&id](const WorkspaceDefinition& value)
        { return value.id == id; });
    return found == state_.namedWorkspaces.end() ? nullptr : &*found;
}

const WorkspaceDefinition* UserWorkspaceService::FindWorkspace(
    const std::string& id) const
{
    const auto found = std::find_if(state_.namedWorkspaces.begin(),
        state_.namedWorkspaces.end(), [&id](const WorkspaceDefinition& value)
        { return value.id == id; });
    return found == state_.namedWorkspaces.end() ? nullptr : &*found;
}

std::string UserWorkspaceService::CurrentIdentity() const
{
    return state_.currentWorkspaceId.empty() ?
        "mode:" + activeModeId_ : state_.currentWorkspaceId;
}

UserWorkspaceOverlay* UserWorkspaceService::FindOverlay(const std::string& id)
{
    const auto found = std::find_if(overlays_.begin(), overlays_.end(),
        [&id](const UserWorkspaceOverlay& value) { return value.workspaceId == id; });
    return found == overlays_.end() ? nullptr : &*found;
}

const UserWorkspaceOverlay* UserWorkspaceService::FindOverlay(
    const std::string& id) const
{
    const auto found = std::find_if(overlays_.begin(), overlays_.end(),
        [&id](const UserWorkspaceOverlay& value) { return value.workspaceId == id; });
    return found == overlays_.end() ? nullptr : &*found;
}

void UserWorkspaceService::RemoveOverlay(const std::string& id)
{
    std::erase_if(overlays_, [&id](const UserWorkspaceOverlay& value)
    { return value.workspaceId == id; });
}

void UserWorkspaceService::QuarantineCorruptFile(const std::string& diagnostic)
{
    state_ = {};
    overlays_.clear();
    state_.recoveredFromCorruption = true;
    state_.diagnostic = diagnostic;
    std::error_code error;
    const auto quarantine = storagePath_.string() + ".corrupt";
    std::filesystem::remove(quarantine, error);
    error.clear();
    std::filesystem::rename(storagePath_, quarantine, error);
}
} // namespace SagaEditor
