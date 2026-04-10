/// @file ComponentObserver.cpp
/// @brief Component observer registry implementation.

#include "SagaEngine/ECS/ComponentObserver.h"

#include <algorithm>

namespace SagaEngine::ECS
{

// ─── Registration ─────────────────────────────────────────────────────────────

ObserverHandle ComponentObserverRegistry::Register(ComponentTypeId     typeId,
                                                    ComponentEvent      event,
                                                    ComponentObserverFn callback)
{
    const ObserverHandle handle = m_nextHandle++;

    Entry entry;
    entry.handle   = handle;
    entry.typeId   = typeId;
    entry.event    = event;
    entry.callback = std::move(callback);

    m_observers.push_back(std::move(entry));
    return handle;
}

ObserverHandle ComponentObserverRegistry::RegisterGlobal(ComponentEvent      event,
                                                          ComponentObserverFn callback)
{
    // INVALID_COMPONENT_TYPE signals "match any type".
    return Register(INVALID_COMPONENT_TYPE, event, std::move(callback));
}

void ComponentObserverRegistry::Unregister(ObserverHandle handle)
{
    m_observers.erase(
        std::remove_if(m_observers.begin(), m_observers.end(),
            [handle](const Entry& e) { return e.handle == handle; }),
        m_observers.end());
}

void ComponentObserverRegistry::Clear()
{
    m_observers.clear();
    m_nextHandle = 1;
}

// ─── Dispatch ─────────────────────────────────────────────────────────────────

void ComponentObserverRegistry::Notify(EntityId        entityId,
                                        ComponentTypeId typeId,
                                        ComponentEvent  event) const
{
    for (const auto& entry : m_observers)
    {
        if (entry.event != event)
            continue;

        // Match if observer is global (INVALID type) or specific type matches.
        if (entry.typeId != INVALID_COMPONENT_TYPE && entry.typeId != typeId)
            continue;

        entry.callback(entityId, typeId, event);
    }
}

// ─── Queries ──────────────────────────────────────────────────────────────────

std::size_t ComponentObserverRegistry::ObserverCount() const noexcept
{
    return m_observers.size();
}

} // namespace SagaEngine::ECS
