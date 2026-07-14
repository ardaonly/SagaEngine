/// @file EditorNotificationCenter.cpp
/// @brief Qt-free transient editor notification center implementation.

#include "SagaEditor/Notifications/EditorNotificationCenter.h"

#include <algorithm>
#include <utility>

namespace SagaEditor
{
namespace
{

constexpr std::size_t kMaxRecentNotifications = 32;

} // namespace

void EditorNotificationCenter::Publish(EditorNotification notification)
{
    m_recent.push_back(notification);
    if (m_recent.size() > kMaxRecentNotifications)
    {
        m_recent.erase(m_recent.begin());
    }

    const auto subscribers = m_subscribers;
    for (const auto& [subscription, callback] : subscribers)
    {
        (void)subscription;
        if (callback)
        {
            callback(notification);
        }
    }
}

EditorNotificationSubscription
EditorNotificationCenter::Subscribe(Callback callback)
{
    if (!callback)
    {
        return kInvalidEditorNotificationSubscription;
    }

    const EditorNotificationSubscription subscription = m_nextSubscription++;
    m_subscribers.push_back({ subscription, std::move(callback) });
    return subscription;
}

void EditorNotificationCenter::Unsubscribe(
    EditorNotificationSubscription subscription)
{
    if (subscription == kInvalidEditorNotificationSubscription)
    {
        return;
    }

    m_subscribers.erase(
        std::remove_if(m_subscribers.begin(),
                       m_subscribers.end(),
                       [subscription](const auto& entry)
                       {
                           return entry.first == subscription;
                       }),
        m_subscribers.end());
}

const std::vector<EditorNotification>&
EditorNotificationCenter::Recent() const noexcept
{
    return m_recent;
}

} // namespace SagaEditor
