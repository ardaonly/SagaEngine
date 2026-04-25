/// @file ShortcutManager.h
/// @brief Manages keyboard shortcut bindings — fully remappable at runtime.

#pragma once

#include <string>
#include <unordered_map>

namespace SagaEditor
{

class CommandDispatcher;

// ─── Key Chord ────────────────────────────────────────────────────────────────

/// Platform-agnostic key chord (modifier mask + key code).
struct KeyChord
{
    uint32_t    modifiers = 0; ///< Bitmask: Ctrl=1, Shift=2, Alt=4, Super=8.
    uint32_t    keyCode   = 0; ///< Platform-agnostic key code (SDL scancode on SDL builds).

    [[nodiscard]] bool operator==(const KeyChord& o) const noexcept
    {
        return modifiers == o.modifiers && keyCode == o.keyCode;
    }
};

/// Hash for KeyChord so it can be used as an unordered_map key.
struct KeyChordHash
{
    [[nodiscard]] std::size_t operator()(const KeyChord& k) const noexcept
    {
        return std::hash<uint64_t>{}(
            (static_cast<uint64_t>(k.modifiers) << 32) | k.keyCode);
    }
};

// ─── Shortcut Manager ─────────────────────────────────────────────────────────

/// Maps keyboard shortcuts to command ids and dispatches on match.
/// Users can remap any binding; bindings are saved in the user profile.
class ShortcutManager
{
public:
    explicit ShortcutManager(CommandDispatcher& dispatcher);

    /// Bind a key chord to a command id. Overwrites any existing binding.
    void Bind(KeyChord chord, const std::string& commandId);

    /// Remove the binding for a chord.
    void Unbind(const KeyChord& chord);

    /// Remove all bindings for a command id.
    void UnbindCommand(const std::string& commandId);

    /// Process a key event. Returns true if a binding was matched and dispatched.
    [[nodiscard]] bool OnKeyEvent(const KeyChord& chord);

    /// Return the command id bound to a chord, or empty string if unbound.
    [[nodiscard]] std::string GetBoundCommand(const KeyChord& chord) const;

    /// Return the first chord bound to a command, or empty chord if none.
    [[nodiscard]] KeyChord GetBoundChord(const std::string& commandId) const;

private:
    CommandDispatcher& m_dispatcher; ///< Non-owning; owned by EditorHost.

    std::unordered_map<KeyChord, std::string, KeyChordHash> m_chordToCommand;
    std::unordered_map<std::string, KeyChord>               m_commandToChord;
};

} // namespace SagaEditor
