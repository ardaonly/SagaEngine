/// @file PersonaSettingsBinding.h
/// @brief Persists the active persona id into a private editor settings store.

#pragma once

#include "SagaEditor/Persona/PersonaRegistry.h"

namespace SagaEditor
{

class IEditorSettingsStore;

/// Binds `PersonaRegistry` to user-private settings. It restores the active
/// persona during startup and writes later persona changes back to the store.
class PersonaSettingsBinding
{
public:
    PersonaSettingsBinding() = default;
    ~PersonaSettingsBinding();

    PersonaSettingsBinding(const PersonaSettingsBinding&) = delete;
    PersonaSettingsBinding& operator=(const PersonaSettingsBinding&) = delete;

    bool Init(PersonaRegistry& registry, IEditorSettingsStore& settings);
    void Shutdown();

    [[nodiscard]] bool IsBound() const noexcept;

private:
    PersonaRegistry*            m_registry = nullptr;
    IEditorSettingsStore*       m_settings = nullptr;
    PersonaChangeSubscription   m_subscription = kInvalidPersonaSubscription;
};

} // namespace SagaEditor
