/// @file PersonaRegistry.cpp
/// @brief Implementation of the persona catalogue + change notification.

#include "SagaEditor/Persona/PersonaRegistry.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

PersonaRegistry::PersonaRegistry() = default;

// ─── Population ───────────────────────────────────────────────────────────────

void PersonaRegistry::RegisterBuiltinPersonas()
{
    Register(MakeBlockyBeginnerPersona());
    Register(MakeIndieBalancedPersona());
    Register(MakeProDensePersona());
    Register(MakeTechnicalPersona());
    Register(MakeCustomPersona());
}

bool PersonaRegistry::Register(UIPersona persona)
{
    if (persona.id.empty())
    {
        return false;
    }
    const std::string key = persona.id;
    m_personas.insert_or_assign(key, std::move(persona));
    return true;
}

bool PersonaRegistry::Unregister(const std::string& id)
{
    const bool removed = m_personas.erase(id) != 0;
    if (removed && id == m_activeId)
    {
        m_activeId.clear();
    }
    return removed;
}

void PersonaRegistry::Clear() noexcept
{
    m_personas.clear();
    m_activeId.clear();
}

// ─── Lookup ───────────────────────────────────────────────────────────────────

bool PersonaRegistry::Has(const std::string& id) const noexcept
{
    return m_personas.find(id) != m_personas.end();
}

std::size_t PersonaRegistry::Size() const noexcept
{
    return m_personas.size();
}

const UIPersona* PersonaRegistry::Find(const std::string& id) const noexcept
{
    const auto it = m_personas.find(id);
    return it == m_personas.end() ? nullptr : &it->second;
}

std::vector<UIPersona> PersonaRegistry::GetAll() const
{
    std::vector<UIPersona> out;
    out.reserve(m_personas.size());
    for (const auto& [_, persona] : m_personas)
    {
        out.push_back(persona);
    }

    // Sort by tier ascending then by id ascending for stable display.
    std::sort(out.begin(), out.end(),
              [](const UIPersona& a, const UIPersona& b) noexcept
              {
                  if (a.tier != b.tier)
                  {
                      return static_cast<std::uint8_t>(a.tier) <
                             static_cast<std::uint8_t>(b.tier);
                  }
                  return a.id < b.id;
              });
    return out;
}

// ─── Activation ───────────────────────────────────────────────────────────────

bool PersonaRegistry::SetActive(const std::string& id)
{
    const auto it = m_personas.find(id);
    if (it == m_personas.end())
    {
        return false;
    }

    m_activeId = id;

    // Snapshot the subscriber list so a callback that re-enters the
    // registry (Subscribe / Unsubscribe / Clear) cannot invalidate the
    // iterator we are walking.
    const auto snapshot = m_subscribers;
    for (const auto& sub : snapshot)
    {
        // Skip subscribers detached during the dispatch.
        const auto live = std::find_if(
            m_subscribers.begin(), m_subscribers.end(),
            [&sub](const Subscriber& s) noexcept { return s.handle == sub.handle; });
        if (live == m_subscribers.end())
        {
            continue;
        }

        try
        {
            sub.callback(it->second);
        }
        catch (...)
        {
            // A misbehaving subscriber must not break further dispatch.
            // Diagnostics belong in the editor logging layer when wired.
        }
    }

    return true;
}

const UIPersona* PersonaRegistry::GetActive() const noexcept
{
    if (m_activeId.empty())
    {
        return nullptr;
    }
    const auto it = m_personas.find(m_activeId);
    return it == m_personas.end() ? nullptr : &it->second;
}

const std::string& PersonaRegistry::GetActiveId() const noexcept
{
    return m_activeId;
}

// ─── Change Notification ──────────────────────────────────────────────────────

PersonaChangeSubscription
PersonaRegistry::OnPersonaChanged(PersonaChangedFn callback)
{
    if (!callback)
    {
        return kInvalidPersonaSubscription;
    }
    Subscriber sub{ m_nextHandle++, std::move(callback) };
    const auto handle = sub.handle;
    m_subscribers.push_back(std::move(sub));
    return handle;
}

bool PersonaRegistry::RemovePersonaChangedCallback(PersonaChangeSubscription handle) noexcept
{
    if (handle == kInvalidPersonaSubscription)
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

std::size_t PersonaRegistry::SubscriberCount() const noexcept
{
    return m_subscribers.size();
}

} // namespace SagaEditor
