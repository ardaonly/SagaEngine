/// @file EditorNotificationCenter.h
/// @brief Qt-free transient editor notification center for user-facing status feedback.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace SagaEditor
{

/// Severity used for transient editor status notifications.
enum class EditorNotificationSeverity
{
    Info,
    Success,
    Warning,
    Error
};

/// User-facing transient notification emitted by editor UX systems.
struct EditorNotification
{
    EditorNotificationSeverity severity = EditorNotificationSeverity::Info; ///< Presentation severity.
    std::string source;  ///< Stable source id for filtering and tests.
    std::string message; ///< Short status-bar friendly text.
    std::string detail;  ///< Optional longer detail for future UI.
};

using EditorNotificationSubscription = std::uint64_t;
inline constexpr EditorNotificationSubscription kInvalidEditorNotificationSubscription = 0;

/// In-process notification hub for transient editor UX feedback.
class EditorNotificationCenter
{
public:
    using Callback = std::function<void(const EditorNotification&)>;

    /// Publish a notification to current subscribers and keep it in recent history.
    void Publish(EditorNotification notification);

    /// Subscribe to future notifications. Returns an opaque subscription id.
    [[nodiscard]] EditorNotificationSubscription Subscribe(Callback callback);

    /// Remove a previous subscription. Unknown ids are ignored.
    void Unsubscribe(EditorNotificationSubscription subscription);

    /// Return recent notifications in publish order.
    [[nodiscard]] const std::vector<EditorNotification>& Recent() const noexcept;

private:
    std::vector<std::pair<EditorNotificationSubscription, Callback>> m_subscribers;
    std::vector<EditorNotification> m_recent;
    EditorNotificationSubscription m_nextSubscription = 1;
};

} // namespace SagaEditor
