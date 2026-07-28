// SPDX-License-Identifier: MPL-2.0
/// @file CommandDispatcher.cpp
/// @brief Routes command invocations to their registered handlers.

#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandRegistry.h"

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

CommandDispatcher::CommandDispatcher(CommandRegistry& registry)
    : m_registry(registry)
{}

// ─── Dispatch ─────────────────────────────────────────────────────────────────

bool CommandDispatcher::Dispatch(const std::string& commandId)
{
    const CommandDescriptor* desc = m_registry.Find(commandId);
    if (!desc || !desc->enabled || !desc->handler)
    {
        for (auto& post : m_postHooks)
            post(commandId, false);
        return false;
    }

    // Pre-hook veto check
    for (auto& pre : m_preHooks)
    {
        if (!pre(commandId))
        {
            for (auto& post : m_postHooks)
                post(commandId, false);
            return false;
        }
    }

    // Execute. A registered command without a handler is not an available
    // workflow and must never be reported as successfully dispatched.
    desc->handler();

    for (auto& post : m_postHooks)
        post(commandId, true);

    return true;
}

// ─── Hooks ────────────────────────────────────────────────────────────────────

void CommandDispatcher::AddPreHook(CommandPreHook hook)
{
    m_preHooks.push_back(std::move(hook));
}

void CommandDispatcher::AddPostHook(CommandPostHook hook)
{
    m_postHooks.push_back(std::move(hook));
}

} // namespace SagaEditor
