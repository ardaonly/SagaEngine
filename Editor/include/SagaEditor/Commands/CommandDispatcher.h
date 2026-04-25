/// @file CommandDispatcher.h
/// @brief Routes command invocations to their registered handlers.

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace SagaEditor
{

class CommandRegistry;

// ─── Pre/Post Hooks ───────────────────────────────────────────────────────────

/// Called before the command handler runs. Return false to cancel the dispatch.
using CommandPreHook  = std::function<bool(const std::string& commandId)>;

/// Called after the command handler completes (regardless of cancel).
using CommandPostHook = std::function<void(const std::string& commandId, bool executed)>;

// ─── Command Dispatcher ───────────────────────────────────────────────────────

/// Invokes command handlers looked up from the registry.
/// Pre-hooks can veto a dispatch; post-hooks observe every invocation.
/// Both hook types are usable by extensions to instrument command flow.
class CommandDispatcher
{
public:
    explicit CommandDispatcher(CommandRegistry& registry);

    /// Invoke the command with the given id. Returns false if the command was
    /// not found, is disabled, or was vetoed by a pre-hook.
    [[nodiscard]] bool Dispatch(const std::string& commandId);

    // ─── Hooks ────────────────────────────────────────────────────────────────

    /// Add a pre-hook. Hooks run in registration order; first false veto wins.
    void AddPreHook(CommandPreHook hook);

    /// Add a post-hook. Runs after every dispatch attempt.
    void AddPostHook(CommandPostHook hook);

private:
    CommandRegistry&              m_registry;   ///< Non-owning; owned by EditorHost.
    std::vector<CommandPreHook>   m_preHooks;
    std::vector<CommandPostHook>  m_postHooks;
};

} // namespace SagaEditor
