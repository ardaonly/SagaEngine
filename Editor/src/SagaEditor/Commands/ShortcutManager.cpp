/// @file ShortcutManager.cpp
/// @brief Manages keyboard shortcut bindings — fully remappable at runtime.

#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Commands/CommandDispatcher.h"

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

ShortcutManager::ShortcutManager(CommandDispatcher& dispatcher)
    : m_dispatcher(dispatcher)
{}

// ─── Binding ──────────────────────────────────────────────────────────────────

void ShortcutManager::Bind(KeyChord chord, const std::string& commandId)
{
    // Remove any previous binding for this chord
    auto existing = m_chordToCommand.find(chord);
    if (existing != m_chordToCommand.end())
        m_commandToChord.erase(existing->second);

    m_chordToCommand[chord]     = commandId;
    m_commandToChord[commandId] = chord;
}

void ShortcutManager::Unbind(const KeyChord& chord)
{
    auto it = m_chordToCommand.find(chord);
    if (it == m_chordToCommand.end())
        return;

    m_commandToChord.erase(it->second);
    m_chordToCommand.erase(it);
}

void ShortcutManager::UnbindCommand(const std::string& commandId)
{
    auto it = m_commandToChord.find(commandId);
    if (it == m_commandToChord.end())
        return;

    m_chordToCommand.erase(it->second);
    m_commandToChord.erase(it);
}

// ─── Event Handling ───────────────────────────────────────────────────────────

bool ShortcutManager::OnKeyEvent(const KeyChord& chord)
{
    auto it = m_chordToCommand.find(chord);
    if (it == m_chordToCommand.end())
        return false;

    return m_dispatcher.Dispatch(it->second);
}

// ─── Queries ──────────────────────────────────────────────────────────────────

std::string ShortcutManager::GetBoundCommand(const KeyChord& chord) const
{
    auto it = m_chordToCommand.find(chord);
    return (it != m_chordToCommand.end()) ? it->second : std::string{};
}

KeyChord ShortcutManager::GetBoundChord(const std::string& commandId) const
{
    auto it = m_commandToChord.find(commandId);
    return (it != m_commandToChord.end()) ? it->second : KeyChord{};
}

} // namespace SagaEditor
