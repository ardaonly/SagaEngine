/// @file InspectorBinder.h
/// @brief Bridges the active selection to the inspector's edit pipeline.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace SagaEditor
{

// ─── Identity ─────────────────────────────────────────────────────────────────

/// Sentinel used by the binder to mean "no entity is bound right now".
inline constexpr std::uint64_t kInvalidInspectorEntity = 0;

// ─── Subscription Handle ──────────────────────────────────────────────────────

/// Opaque handle returned by `Subscribe`. Callers pass the same handle
/// to `Unsubscribe` to stop receiving change notifications. Handles
/// are monotonically increasing so 0 is reserved for "not a real
/// subscription".
using PropertyChangeSubscription = std::uint64_t;

/// Sentinel returned by `Subscribe` when the callback was empty and
/// nothing was actually registered.
inline constexpr PropertyChangeSubscription kInvalidSubscription = 0;

// ─── Inspector Binder ─────────────────────────────────────────────────────────

/// Holds the currently bound entity id and forwards property-change
/// edits from the inspector widgets to one or more subscribers
/// (typically the undo system, the live preview, and the collaboration
/// sync layer). The binder itself is intentionally Qt-free so the
/// inspector backend can swap its widget toolkit without rewriting the
/// edit pipeline.
class InspectorBinder
{
public:
    /// Callback signature: `path` is a dot-separated property path
    /// (`"transform.position.x"`) and `value` is a serialised string
    /// representation. The text form is used so the binder does not
    /// have to know every possible property type at compile time —
    /// the subscriber decodes the value against its own schema.
    using PropertyChangedFn =
        std::function<void(const std::string& path,
                           const std::string& value)>;

    InspectorBinder()  = default;
    ~InspectorBinder() = default;

    InspectorBinder(const InspectorBinder&)            = delete;
    InspectorBinder& operator=(const InspectorBinder&) = delete;
    InspectorBinder(InspectorBinder&&)                 = default;
    InspectorBinder& operator=(InspectorBinder&&)      = default;

    // ─── Selection Binding ────────────────────────────────────────────────────

    /// Bind the inspector to a single entity. Re-binding to the same
    /// id is a no-op; binding to a different id invalidates any
    /// in-flight edit state held by subscribers (the inspector
    /// rebuilds its widget tree on selection change anyway).
    void BindEntity(std::uint64_t entityId) noexcept;

    /// Drop the current binding. Equivalent to
    /// `BindEntity(kInvalidInspectorEntity)`.
    void UnbindAll() noexcept;

    /// Currently bound entity id, or `kInvalidInspectorEntity` when
    /// no selection is active.
    [[nodiscard]] std::uint64_t GetBoundEntity() const noexcept;

    /// Convenience predicate matching the sentinel.
    [[nodiscard]] bool HasBinding() const noexcept;

    // ─── Subscriptions ────────────────────────────────────────────────────────

    /// Subscribe a callback to property-change notifications. Returns
    /// a handle the caller stores; pass the handle to `Unsubscribe` to
    /// detach. An empty `std::function` is rejected and
    /// `kInvalidSubscription` is returned.
    PropertyChangeSubscription Subscribe(PropertyChangedFn callback);

    /// Detach a previously registered subscription. Returns true when
    /// the handle named a live subscriber, false when it did not
    /// (already removed, never registered, or the sentinel).
    bool Unsubscribe(PropertyChangeSubscription handle) noexcept;

    /// Drop every subscriber. Used by the inspector during shutdown
    /// and by tests between cases.
    void ClearSubscribers() noexcept;

    /// Number of live subscribers. Useful for sanity assertions and
    /// for diagnostics overlays.
    [[nodiscard]] std::size_t SubscriberCount() const noexcept;

    // ─── Edit Forwarding ──────────────────────────────────────────────────────

    /// Fan out a property edit to every subscriber, in subscription
    /// order. Subscribers that throw are caught and dropped — the
    /// inspector cannot afford a single misbehaving listener to break
    /// the whole edit pipeline.
    void NotifyPropertyChanged(const std::string& path,
                               const std::string& value);

private:
    // ─── Subscriber Storage ───────────────────────────────────────────────────

    /// Per-subscriber record. The handle is monotonically increasing
    /// so erasing one entry never invalidates other handles.
    struct Subscriber
    {
        PropertyChangeSubscription handle = kInvalidSubscription; ///< Stable id.
        PropertyChangedFn          callback;                      ///< Listener.
    };

    std::uint64_t            m_entity        = kInvalidInspectorEntity; ///< Bound entity.
    std::uint64_t            m_nextHandle    = kInvalidSubscription + 1; ///< Next id.
    std::vector<Subscriber>  m_subscribers;                              ///< Listeners.
};

} // namespace SagaEditor
