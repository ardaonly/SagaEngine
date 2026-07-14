/// @file EditorProfileSettingsBinding.cpp
/// @brief Persists the active editor workflow profile id.

#include "SagaEditor/Profile/EditorProfileSettingsBinding.h"

#include "SagaEditor/Settings/IEditorSettingsStore.h"

namespace SagaEditor
{

namespace
{

constexpr const char* kActiveProfileSettingKey = "profile.activeId";

} // namespace

EditorProfileSettingsBinding::~EditorProfileSettingsBinding()
{
    Shutdown();
}

bool EditorProfileSettingsBinding::Init(EditorProfileRegistry& registry,
                                        IEditorSettingsStore& settings)
{
    Shutdown();

    m_registry = &registry;
    m_settings = &settings;

    const std::string storedProfile =
        m_settings->GetString(kActiveProfileSettingKey, "");
    std::string activeProfile =
        storedProfile.empty() ? DefaultEditorProfileId()
                              : ResolveEditorProfileId(storedProfile);

    if (!m_registry->Has(activeProfile))
    {
        activeProfile = DefaultEditorProfileId();
        m_settings->SetString(kActiveProfileSettingKey, activeProfile);
        m_settings->Flush();
    }
    else if (!storedProfile.empty() && storedProfile != activeProfile)
    {
        m_settings->SetString(kActiveProfileSettingKey, activeProfile);
        m_settings->Flush();
    }

    if (!m_registry->SetActive(activeProfile))
    {
        return false;
    }

    m_subscription = m_registry->OnProfileChanged(
        [this](const EditorProfile& profile)
        {
            m_settings->SetString(kActiveProfileSettingKey, profile.id);
            m_settings->Flush();
        });

    return m_subscription != kInvalidEditorProfileSubscription;
}

void EditorProfileSettingsBinding::Shutdown()
{
    if (m_registry && m_subscription != kInvalidEditorProfileSubscription)
    {
        m_registry->RemoveProfileChangedCallback(m_subscription);
    }

    m_subscription = kInvalidEditorProfileSubscription;
    m_registry = nullptr;
    m_settings = nullptr;
}

bool EditorProfileSettingsBinding::IsBound() const noexcept
{
    return m_registry != nullptr &&
           m_settings != nullptr &&
           m_subscription != kInvalidEditorProfileSubscription;
}

} // namespace SagaEditor
