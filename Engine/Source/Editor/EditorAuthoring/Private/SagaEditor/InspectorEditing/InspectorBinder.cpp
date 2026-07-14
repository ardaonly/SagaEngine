/// @file InspectorBinder.cpp
/// @brief Implementation of the inspector selection-and-edit forwarder.

#include "SagaEditor/InspectorEditing/InspectorBinder.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace SagaEditor
{

// ─── Selection Binding ────────────────────────────────────────────────────────

void InspectorBinder::BindEntity(std::uint64_t entityId) noexcept
{
    m_entity = entityId;
}

void InspectorBinder::UnbindAll() noexcept
{
    m_entity = kInvalidInspectorEntity;
}

std::uint64_t InspectorBinder::GetBoundEntity() const noexcept
{
    return m_entity;
}

bool InspectorBinder::HasBinding() const noexcept
{
    return m_entity != kInvalidInspectorEntity;
}

// ─── Subscriptions ────────────────────────────────────────────────────────────

PropertyChangeSubscription
InspectorBinder::Subscribe(PropertyChangedFn callback)
{
    if (!callback)
    {
        // Empty std::function is rejected so NotifyPropertyChanged
        // never has to branch on per-subscriber validity.
        return kInvalidSubscription;
    }

    Subscriber sub{ m_nextHandle++, std::move(callback) };
    const auto handle = sub.handle;
    m_subscribers.push_back(std::move(sub));
    return handle;
}

bool InspectorBinder::Unsubscribe(PropertyChangeSubscription handle) noexcept
{
    if (handle == kInvalidSubscription)
    {
        return false;
    }

    const auto it = std::find_if(m_subscribers.begin(),
                                 m_subscribers.end(),
                                 [handle](const Subscriber& s) noexcept
                                 { return s.handle == handle; });

    if (it == m_subscribers.end())
    {
        return false;
    }

    m_subscribers.erase(it);
    return true;
}

void InspectorBinder::ClearSubscribers() noexcept
{
    m_subscribers.clear();
}

std::size_t InspectorBinder::SubscriberCount() const noexcept
{
    return m_subscribers.size();
}

// ─── Edit Forwarding ──────────────────────────────────────────────────────────

void InspectorBinder::NotifyPropertyChanged(const std::string& path,
                                            const std::string& value)
{
    // Iterate over an indexed copy of the subscriber list so a callback
    // that re-enters the binder (Subscribe / Unsubscribe / ClearSubscribers)
    // cannot invalidate the iterator we are walking. The handles stored
    // in the snapshot are only used to detect listeners that have since
    // unsubscribed during the dispatch.
    const auto snapshot = m_subscribers;

    for (const auto& sub : snapshot)
    {
        // Skip subscribers that were unsubscribed during a previous
        // dispatch in the same call chain.
        const auto stillLive =
            std::find_if(m_subscribers.begin(),
                         m_subscribers.end(),
                         [&sub](const Subscriber& s) noexcept
                         { return s.handle == sub.handle; });
        if (stillLive == m_subscribers.end())
        {
            continue;
        }

        try
        {
            sub.callback(path, value);
        }
        catch (...)
        {
            // A misbehaving subscriber must not break the rest of the
            // edit pipeline. We silently swallow the exception; richer
            // diagnostics belong in the editor logging layer once it
            // is wired in.
        }
    }
}

} // namespace SagaEditor
