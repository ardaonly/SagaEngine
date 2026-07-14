/// @file CommandRegistry.cpp
/// @brief Registry of all named editor commands — the single source of truth for actions.

#include "SagaEditor/Commands/CommandRegistry.h"
#include <algorithm>

namespace SagaEditor
{

// ─── Registration ─────────────────────────────────────────────────────────────

void CommandRegistry::Register(CommandDescriptor descriptor)
{
    m_commands[descriptor.id] = std::move(descriptor);
}

void CommandRegistry::Unregister(const std::string& id)
{
    m_commands.erase(id);
}

// ─── Lookup ───────────────────────────────────────────────────────────────────

const CommandDescriptor* CommandRegistry::Find(const std::string& id) const
{
    auto it = m_commands.find(id);
    return (it != m_commands.end()) ? &it->second : nullptr;
}

std::vector<const CommandDescriptor*> CommandRegistry::GetAll(
    const std::string& category) const
{
    std::vector<const CommandDescriptor*> result;
    result.reserve(m_commands.size());

    for (const auto& [id, desc] : m_commands)
    {
        if (category.empty() || desc.category == category)
            result.push_back(&desc);
    }

    // Stable alphabetical order for predictable palette rendering.
    std::sort(result.begin(), result.end(),
        [](const CommandDescriptor* a, const CommandDescriptor* b) {
            return a->label < b->label;
        });

    return result;
}

// ─── Mutation ─────────────────────────────────────────────────────────────────

void CommandRegistry::SetEnabled(const std::string& id, bool enabled)
{
    auto it = m_commands.find(id);
    if (it != m_commands.end())
        it->second.enabled = enabled;
}

} // namespace SagaEditor
