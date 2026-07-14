/// @file PersonaRegistry.h
/// @brief Owns the catalog of available UI personas — built-in + user-authored.

#pragma once

#include "SagaEditor/Persona/UIPersona.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditor
{

// ─── Subscription Handle ──────────────────────────────────────────────────────

/// Opaque handle the registry returns from `OnPersonaChanged`. Pass
/// it back to `RemovePersonaChangedCallback` to detach. Sentinel `0`
/// means "not a real subscription".
using PersonaChangeSubscription = std::uint64_t;
inline constexpr PersonaChangeSubscription kInvalidPersonaSubscription = 0;

// ─── Persona Registry ─────────────────────────────────────────────────────────

/// Catalogue of every `UIPersona` known to the editor. The registry
/// answers two distinct questions: what personas are available right
/// now, and which one is currently active. Switching the active
/// persona fires every registered callback so the activator (and any
/// other interested subsystem like the welcome screen) can react.
///
/// The registry is intentionally framework-free — no Qt, no theme, no
/// layout serializer dependencies — so the welcome screen can list
/// personas before any UI backend has been instantiated.
class PersonaRegistry
{
public:
    /// Callback signature: receives the persona that was just made
    /// active. The previous persona is reachable through the
    /// callback's own captured state if the consumer needs it.
    using PersonaChangedFn = std::function<void(const UIPersona&)>;

    PersonaRegistry();
    ~PersonaRegistry() = default;

    PersonaRegistry(const PersonaRegistry&)            = delete;
    PersonaRegistry& operator=(const PersonaRegistry&) = delete;
    PersonaRegistry(PersonaRegistry&&)                 = default;
    PersonaRegistry& operator=(PersonaRegistry&&)      = default;

    // ─── Population ───────────────────────────────────────────────────────────

    /// Add the five engine-shipped personas. Idempotent — re-registering
    /// the same id replaces the previous entry.
    void RegisterBuiltinPersonas();

    /// Add or replace a persona. The persona's `id` is used as the key.
    /// Returns false when `id` is empty (the registry refuses unnamed
    /// personas to keep diagnostics actionable).
    bool Register(UIPersona persona);

    /// Remove a persona by id. Returns true when an entry was removed.
    /// Removing the active persona resets `m_activeId` to empty;
    /// callers should pick a new active persona afterwards.
    bool Unregister(const std::string& id);

    /// Clear every persona and reset the active id. Used by tests.
    void Clear() noexcept;

    // ─── Lookup ───────────────────────────────────────────────────────────────

    [[nodiscard]] bool        Has(const std::string& id) const noexcept;
    [[nodiscard]] std::size_t Size() const noexcept;

    /// Return a pointer to the persona with the given id, or nullptr
    /// when no such persona is registered. Pointer remains valid until
    /// the matching `Unregister` or `Clear`, or until another
    /// `Register` call rehashes the underlying map.
    [[nodiscard]] const UIPersona* Find(const std::string& id) const noexcept;

    /// Snapshot of every registered persona, sorted by tier ascending
    /// then by id ascending. Returned by value so the caller cannot
    /// hold a reference into the table across later registrations.
    [[nodiscard]] std::vector<UIPersona> GetAll() const;

    // ─── Activation ───────────────────────────────────────────────────────────

    /// Switch the active persona. Returns false when `id` is unknown.
    /// Fires every registered callback with the new persona on success.
    bool SetActive(const std::string& id);

    /// Currently active persona, or nullptr when none has been picked
    /// yet. The editor host sets the initial persona at start-up and
    /// the welcome screen overrides it on first launch.
    [[nodiscard]] const UIPersona* GetActive() const noexcept;

    /// Active persona id, or an empty string when none is active.
    [[nodiscard]] const std::string& GetActiveId() const noexcept;

    // ─── Change Notification ──────────────────────────────────────────────────

    /// Subscribe to active-persona changes. Returns a handle the
    /// caller passes to `RemovePersonaChangedCallback`. Empty
    /// callbacks are rejected and `kInvalidPersonaSubscription` is
    /// returned.
    PersonaChangeSubscription OnPersonaChanged(PersonaChangedFn callback);

    /// Detach a previously installed callback. Returns true when the
    /// handle named a live subscriber.
    bool RemovePersonaChangedCallback(PersonaChangeSubscription handle) noexcept;

    /// Number of live change subscribers.
    [[nodiscard]] std::size_t SubscriberCount() const noexcept;

private:
    // ─── Internal Storage ─────────────────────────────────────────────────────

    struct Subscriber
    {
        PersonaChangeSubscription handle = kInvalidPersonaSubscription;
        PersonaChangedFn          callback;
    };

    std::unordered_map<std::string, UIPersona> m_personas;
    std::string                                m_activeId;
    std::uint64_t                              m_nextHandle = kInvalidPersonaSubscription + 1;
    std::vector<Subscriber>                    m_subscribers;
};

} // namespace SagaEditor
