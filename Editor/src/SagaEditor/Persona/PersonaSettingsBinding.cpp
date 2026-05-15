/// @file PersonaSettingsBinding.cpp
/// @brief Persists the active persona id into a private editor settings store.

#include "SagaEditor/Persona/PersonaSettingsBinding.h"

#include "SagaEditor/Settings/IEditorSettingsStore.h"

namespace SagaEditor
{

namespace
{

constexpr const char* kActivePersonaSettingKey = "persona.activeId";
constexpr const char* kDefaultPersonaId = "saga.persona.indie";

} // namespace

PersonaSettingsBinding::~PersonaSettingsBinding()
{
    Shutdown();
}

bool PersonaSettingsBinding::Init(PersonaRegistry& registry,
                                  IEditorSettingsStore& settings)
{
    Shutdown();

    m_registry = &registry;
    m_settings = &settings;

    const std::string storedPersona =
        m_settings->GetString(kActivePersonaSettingKey, "");
    std::string activePersona = storedPersona.empty() ? kDefaultPersonaId : storedPersona;

    if (!m_registry->Has(activePersona))
    {
        activePersona = kDefaultPersonaId;
        m_settings->SetString(kActivePersonaSettingKey, activePersona);
        m_settings->Flush();
    }

    if (!m_registry->SetActive(activePersona))
    {
        return false;
    }

    m_subscription = m_registry->OnPersonaChanged(
        [this](const UIPersona& persona)
        {
            m_settings->SetString(kActivePersonaSettingKey, persona.id);
            m_settings->Flush();
        });

    return m_subscription != kInvalidPersonaSubscription;
}

void PersonaSettingsBinding::Shutdown()
{
    if (m_registry && m_subscription != kInvalidPersonaSubscription)
    {
        m_registry->RemovePersonaChangedCallback(m_subscription);
    }

    m_subscription = kInvalidPersonaSubscription;
    m_registry = nullptr;
    m_settings = nullptr;
}

bool PersonaSettingsBinding::IsBound() const noexcept
{
    return m_registry != nullptr &&
           m_settings != nullptr &&
           m_subscription != kInvalidPersonaSubscription;
}

} // namespace SagaEditor
