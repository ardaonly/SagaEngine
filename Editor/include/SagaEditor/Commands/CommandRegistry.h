/// @file CommandRegistry.h
/// @brief Registry of all named editor commands — the single source of truth for actions.

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditor
{

// ─── Command Descriptor ───────────────────────────────────────────────────────

/// Everything the editor needs to know about a command at registration time.
struct CommandDescriptor
{
    std::string           id;          ///< Stable dot-namespaced ID (e.g. "saga.edit.undo").
    std::string           label;       ///< Human-readable label shown in UI and palette.
    std::string           category;    ///< Grouping label for the command palette.
    std::function<void()> handler;     ///< Invocation callback; may be null for stub commands.
    bool                  enabled = true; ///< Commands with enabled=false appear greyed-out.
};

// ─── Command Registry ─────────────────────────────────────────────────────────

/// Centralised command table. Every action in the editor — built-in or
/// package-contributed — registers here. The command palette, shortcut manager,
/// and menu system all read from this registry.
class CommandRegistry
{
public:
    /// Register a new command. Overwrites a prior registration with the same id.
    void Register(CommandDescriptor descriptor);

    /// Remove a command by id (used when an extension unloads).
    void Unregister(const std::string& id);

    /// Look up a command by id. Returns nullptr if not found.
    [[nodiscard]] const CommandDescriptor* Find(const std::string& id) const;

    /// Return all commands, optionally filtered to one category.
    [[nodiscard]] std::vector<const CommandDescriptor*> GetAll(
        const std::string& category = {}) const;

    /// Enable or disable a command without unregistering it.
    void SetEnabled(const std::string& id, bool enabled);

private:
    std::unordered_map<std::string, CommandDescriptor> m_commands; ///< id → descriptor.
};

} // namespace SagaEditor
