/// @file ShortcutCustomizationController.cpp
/// @brief Safe shortcut customization controller implementation.

#include "SagaEditor/Customization/ShortcutCustomizationController.h"

#include "SagaEditor/Composition/ResolvedEditorCompositionSnapshot.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <utility>

namespace SagaEditor
{
namespace
{

[[nodiscard]] bool SameShortcutOverride(
    const ShortcutCustomizationOverride& value,
    const std::string& actionId)
{
    return value.actionId == actionId;
}

[[nodiscard]] std::vector<std::string> Split(const std::string& text,
                                             char delimiter)
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

[[nodiscard]] bool IsValidShortcutChord(const std::string& chord)
{
    if (chord.empty())
    {
        return true;
    }

    bool sawKey = false;
    for (const std::string& token : Split(chord, '+'))
    {
        if (token.empty())
        {
            return false;
        }
        if (token == "Ctrl" || token == "Shift" ||
            token == "Alt" || token == "Super")
        {
            continue;
        }
        if (sawKey || token.size() != 1 ||
            !std::isalnum(static_cast<unsigned char>(token.front())))
        {
            return false;
        }
        sawKey = true;
    }

    return sawKey;
}

[[nodiscard]] std::map<std::string, std::string> BuildCurrentShortcutMap(
    const ResolvedEditorCompositionSnapshot* snapshot)
{
    std::map<std::string, std::string> shortcuts;
    if (snapshot == nullptr)
    {
        return shortcuts;
    }

    for (const ShortcutBindingDefinition& shortcut : snapshot->shortcuts)
    {
        shortcuts[shortcut.actionId] = shortcut.chord;
    }
    return shortcuts;
}

} // namespace

ShortcutCustomizationController::ShortcutCustomizationController(
    ShortcutCustomizationControllerConfig config)
    : m_capabilities(BuildEditorCustomizationCapabilities(config.snapshot,
                                                          config.availability))
    , m_snapshot(config.snapshot)
    , m_overlayId(std::move(config.overlayId))
{
    if (config.initialOverlay)
    {
        m_passthroughLayout = config.initialOverlay->layoutOverrides;
        m_passthroughVisibility = config.initialOverlay->visibilityOverrides;
        m_passthroughToolbar = config.initialOverlay->toolbarOverrides;

        if (config.initialOverlay->baseCompositionId !=
            m_capabilities.compositionId)
        {
            AddDiagnostic(EditorCustomizationDiagnosticSeverity::Blocker,
                          "Customization.InvalidOverlay",
                          "Initial shortcut customization overlay targets a different base composition.",
                          config.initialOverlay->baseCompositionId);
        }
        else
        {
            m_pendingShortcuts = config.initialOverlay->shortcutOverrides;
        }
    }

    RebuildModel();
}

bool ShortcutCustomizationController::SetShortcut(
    const std::string& actionId,
    const std::string& chord)
{
    if (!ValidateChordForAction(actionId, chord))
    {
        RebuildModel();
        return false;
    }

    auto it = std::find_if(m_pendingShortcuts.begin(),
                           m_pendingShortcuts.end(),
                           [&](const ShortcutCustomizationOverride& value)
                           {
                               return SameShortcutOverride(value, actionId);
                           });
    if (chord == CurrentChordForAction(actionId))
    {
        if (it != m_pendingShortcuts.end())
        {
            m_pendingShortcuts.erase(it);
        }
    }
    else if (it != m_pendingShortcuts.end())
    {
        it->chord = chord;
    }
    else
    {
        m_pendingShortcuts.push_back({ actionId, chord });
    }

    RebuildModel();
    return true;
}

bool ShortcutCustomizationController::ClearShortcut(const std::string& actionId)
{
    return SetShortcut(actionId, {});
}

bool ShortcutCustomizationController::ResetShortcut(const std::string& actionId)
{
    if (FindActionState(actionId) == nullptr)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.UnknownAction",
                      "Cannot reset an unknown shortcut action.",
                      actionId);
        RebuildModel();
        return false;
    }

    const auto oldSize = m_pendingShortcuts.size();
    m_pendingShortcuts.erase(
        std::remove_if(m_pendingShortcuts.begin(),
                       m_pendingShortcuts.end(),
                       [&](const ShortcutCustomizationOverride& value)
                       {
                           return SameShortcutOverride(value, actionId);
                       }),
        m_pendingShortcuts.end());
    RebuildModel();
    return oldSize != m_pendingShortcuts.size();
}

void ShortcutCustomizationController::ResetAllShortcuts()
{
    m_pendingShortcuts.clear();
    RebuildModel();
}

const ShortcutCustomizationModel&
ShortcutCustomizationController::GetModel() const noexcept
{
    return m_model;
}

const std::vector<EditorCustomizationDiagnostic>&
ShortcutCustomizationController::GetDiagnostics() const noexcept
{
    return m_diagnostics;
}

bool ShortcutCustomizationController::Validate()
{
    bool valid = m_capabilities.hasUsableSnapshot;
    if (!valid)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Blocker,
                      "Customization.NoCompositionSnapshot",
                      "Cannot validate shortcut customization without a usable snapshot.");
    }

    std::map<std::string, std::string> chordOwners;
    for (const ShortcutCustomizationOverride& overrideValue :
         m_pendingShortcuts)
    {
        if (!ValidateChordForAction(overrideValue.actionId,
                                    overrideValue.chord))
        {
            valid = false;
            continue;
        }
        if (overrideValue.chord.empty())
        {
            continue;
        }

        auto [it, inserted] =
            chordOwners.emplace(overrideValue.chord, overrideValue.actionId);
        if (!inserted && it->second != overrideValue.actionId)
        {
            valid = false;
            AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                          "Customization.ShortcutCollision",
                          "Shortcut chord is assigned to more than one action.",
                          overrideValue.chord);
        }
    }

    RebuildModel();
    return valid && !HasErrorCustomizationDiagnostic(m_diagnostics);
}

EditorCustomizationOverlay
ShortcutCustomizationController::BuildOverlayDelta() const
{
    EditorCustomizationOverlay overlay;
    overlay.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    overlay.baseCompositionId = m_capabilities.compositionId;
    overlay.baseArtifactHash = m_capabilities.artifactHash;
    overlay.overlayId = m_overlayId;
    overlay.layoutOverrides = m_passthroughLayout;
    overlay.visibilityOverrides = m_passthroughVisibility;
    overlay.shortcutOverrides = m_pendingShortcuts;
    overlay.toolbarOverrides = m_passthroughToolbar;
    return overlay;
}

void ShortcutCustomizationController::RebuildModel()
{
    ShortcutCustomizationModel model;
    model.compositionId = m_capabilities.compositionId;
    model.artifactHash = m_capabilities.artifactHash;
    model.diagnostics = m_capabilities.diagnostics;
    model.diagnostics.insert(model.diagnostics.end(),
                             m_diagnostics.begin(),
                             m_diagnostics.end());
    model.status = !m_capabilities.hasUsableSnapshot
                       ? ShortcutCustomizationStatus::Unconfigured
                       : HasErrorCustomizationDiagnostic(model.diagnostics)
                             ? ShortcutCustomizationStatus::Invalid
                             : ShortcutCustomizationStatus::Ready;

    const auto currentShortcuts = BuildCurrentShortcutMap(m_snapshot);
    for (const ActionCustomizationCapability& capability :
         m_capabilities.actions)
    {
        ShortcutCustomizationActionState action;
        action.actionId = capability.actionId;
        action.displayName = capability.displayName.empty()
                                 ? capability.actionId
                                 : capability.displayName;
        action.currentChord = CurrentChordForAction(capability.actionId);
        action.editable = capability.productSafe &&
                          capability.commandImplementationAvailable &&
                          capability.shortcutAssignable;
        if (capability.internalOnly)
        {
            action.lockedReason = EditorCustomizationLockReason::InternalOnly;
        }
        else if (!capability.productSafe)
        {
            action.lockedReason = EditorCustomizationLockReason::NotProductSafe;
        }
        else if (!capability.commandImplementationAvailable ||
                 !capability.shortcutAssignable)
        {
            action.lockedReason =
                EditorCustomizationLockReason::UnavailableImplementation;
        }

        auto pending = std::find_if(
            m_pendingShortcuts.begin(),
            m_pendingShortcuts.end(),
            [&](const ShortcutCustomizationOverride& value)
            {
                return SameShortcutOverride(value, capability.actionId);
            });
        if (pending != m_pendingShortcuts.end())
        {
            action.pendingChord = pending->chord;
        }

        auto current = currentShortcuts.find(capability.actionId);
        if (current != currentShortcuts.end())
        {
            action.currentChord = current->second;
        }

        model.actions.push_back(std::move(action));
    }

    model.dirty = !m_pendingShortcuts.empty();
    model.liveApplyAvailable = false;
    model.restartRequired = model.dirty;
    m_model = std::move(model);
}

ShortcutCustomizationActionState*
ShortcutCustomizationController::FindActionState(const std::string& actionId)
{
    auto it = std::find_if(m_model.actions.begin(),
                           m_model.actions.end(),
                           [&](const ShortcutCustomizationActionState& action)
                           {
                               return action.actionId == actionId;
                           });
    return it == m_model.actions.end() ? nullptr : &*it;
}

const ActionCustomizationCapability*
ShortcutCustomizationController::FindActionCapability(
    const std::string& actionId) const
{
    auto it = std::find_if(m_capabilities.actions.begin(),
                           m_capabilities.actions.end(),
                           [&](const ActionCustomizationCapability& action)
                           {
                               return action.actionId == actionId;
                           });
    return it == m_capabilities.actions.end() ? nullptr : &*it;
}

std::string ShortcutCustomizationController::CurrentChordForAction(
    const std::string& actionId) const
{
    if (m_snapshot == nullptr)
    {
        return {};
    }

    auto it = std::find_if(m_snapshot->shortcuts.begin(),
                           m_snapshot->shortcuts.end(),
                           [&](const ShortcutBindingDefinition& shortcut)
                           {
                               return shortcut.actionId == actionId;
                           });
    return it == m_snapshot->shortcuts.end() ? std::string{} : it->chord;
}

bool ShortcutCustomizationController::ValidateChordForAction(
    const std::string& actionId,
    const std::string& chord)
{
    const ActionCustomizationCapability* capability =
        FindActionCapability(actionId);
    if (capability == nullptr)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.UnknownAction",
                      "Cannot customize an unknown shortcut action.",
                      actionId);
        return false;
    }
    if (capability->internalOnly)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InternalOnlyAction",
                      "Cannot customize an internal-only shortcut action.",
                      actionId);
        return false;
    }
    if (!capability->productSafe)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.ActionLocked",
                      "Cannot customize a shortcut action that is not product-safe.",
                      actionId);
        return false;
    }
    if (!capability->commandImplementationAvailable ||
        !capability->shortcutAssignable)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.UnavailableCommandImplementation",
                      "Cannot customize a shortcut without an available command implementation.",
                      actionId);
        return false;
    }
    if (!IsValidShortcutChord(chord))
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InvalidShortcutChord",
                      "Shortcut chord is not in the supported V1 chord format.",
                      chord);
        return false;
    }

    for (const ShortcutCustomizationOverride& pending : m_pendingShortcuts)
    {
        if (pending.actionId != actionId && !chord.empty() &&
            pending.chord == chord)
        {
            AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                          "Customization.ShortcutCollision",
                          "Shortcut chord is already pending for another action.",
                          chord);
            return false;
        }
    }
    if (m_snapshot != nullptr && !chord.empty())
    {
        for (const ShortcutBindingDefinition& shortcut : m_snapshot->shortcuts)
        {
            const bool overridden = std::any_of(
                m_pendingShortcuts.begin(),
                m_pendingShortcuts.end(),
                [&](const ShortcutCustomizationOverride& pending)
                {
                    return pending.actionId == shortcut.actionId;
                });
            if (shortcut.actionId != actionId && !overridden &&
                shortcut.chord == chord)
            {
                AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                              "Customization.ShortcutCollision",
                              "Shortcut chord is already assigned to another action.",
                              chord);
                return false;
            }
        }
    }
    return true;
}

void ShortcutCustomizationController::AddDiagnostic(
    EditorCustomizationDiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string targetId)
{
    AddCustomizationDiagnostic(m_diagnostics,
                               severity,
                               std::move(code),
                               std::move(message),
                               std::move(targetId));
}

} // namespace SagaEditor
