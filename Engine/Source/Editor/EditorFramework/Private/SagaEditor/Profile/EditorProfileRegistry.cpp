/// @file EditorProfileRegistry.cpp
/// @brief Editor workflow profile catalogue implementation.

#include "SagaEditor/Profile/EditorProfileRegistry.h"

#include <algorithm>
#include <utility>

namespace SagaEditor
{

EditorProfileRegistry::EditorProfileRegistry() = default;

void EditorProfileRegistry::RegisterBuiltinProfiles()
{
    Register(MakeBasicProfile());
    Register(MakeNodeEditorProfile());
    Register(MakeStandardPipelineProfile());
    Register(MakeAdvancedPipelineProfile());
    Register(MakeCustomProfile());
}

bool EditorProfileRegistry::Register(EditorProfile profile)
{
    if (profile.id.empty())
    {
        return false;
    }
    const std::string key = profile.id;
    m_profiles.insert_or_assign(key, std::move(profile));
    return true;
}

bool EditorProfileRegistry::Unregister(const std::string& id)
{
    const bool removed = m_profiles.erase(id) != 0;
    if (removed && id == m_activeId)
    {
        m_activeId.clear();
    }
    return removed;
}

void EditorProfileRegistry::Clear() noexcept
{
    m_profiles.clear();
    m_activeId.clear();
}

bool EditorProfileRegistry::Has(const std::string& id) const
{
    return m_profiles.find(ResolveEditorProfileId(id)) != m_profiles.end();
}

std::size_t EditorProfileRegistry::Size() const noexcept
{
    return m_profiles.size();
}

const EditorProfile* EditorProfileRegistry::Find(const std::string& id) const
{
    const auto it = m_profiles.find(ResolveEditorProfileId(id));
    return it == m_profiles.end() ? nullptr : &it->second;
}

std::vector<EditorProfile> EditorProfileRegistry::GetAll() const
{
    std::vector<EditorProfile> out;
    out.reserve(m_profiles.size());
    for (const auto& [_, profile] : m_profiles)
    {
        out.push_back(profile);
    }

    std::sort(out.begin(), out.end(),
              [](const EditorProfile& a, const EditorProfile& b) noexcept
              {
                  return a.id < b.id;
              });
    return out;
}

bool EditorProfileRegistry::SetActive(const std::string& id)
{
    const std::string resolvedId = ResolveEditorProfileId(id);
    const auto it = m_profiles.find(resolvedId);
    if (it == m_profiles.end())
    {
        return false;
    }

    if (m_activeId == resolvedId)
    {
        return true;
    }

    m_activeId = resolvedId;

    const auto snapshot = m_subscribers;
    for (const auto& sub : snapshot)
    {
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
        }
    }

    return true;
}

const EditorProfile* EditorProfileRegistry::GetActive() const noexcept
{
    if (m_activeId.empty())
    {
        return nullptr;
    }
    const auto it = m_profiles.find(m_activeId);
    return it == m_profiles.end() ? nullptr : &it->second;
}

const std::string& EditorProfileRegistry::GetActiveId() const noexcept
{
    return m_activeId;
}

EditorProfileChangeSubscription
EditorProfileRegistry::OnProfileChanged(ProfileChangedFn callback)
{
    if (!callback)
    {
        return kInvalidEditorProfileSubscription;
    }
    Subscriber sub{ m_nextHandle++, std::move(callback) };
    const auto handle = sub.handle;
    m_subscribers.push_back(std::move(sub));
    return handle;
}

bool EditorProfileRegistry::RemoveProfileChangedCallback(
    EditorProfileChangeSubscription handle) noexcept
{
    if (handle == kInvalidEditorProfileSubscription)
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

std::size_t EditorProfileRegistry::SubscriberCount() const noexcept
{
    return m_subscribers.size();
}

} // namespace SagaEditor
