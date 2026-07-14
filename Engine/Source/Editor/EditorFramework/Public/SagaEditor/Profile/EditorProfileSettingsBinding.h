/// @file EditorProfileSettingsBinding.h
/// @brief Persists the active editor workflow profile id.

#pragma once

#include "SagaEditor/Profile/EditorProfileRegistry.h"

namespace SagaEditor
{

class IEditorSettingsStore;

class EditorProfileSettingsBinding
{
public:
    EditorProfileSettingsBinding() = default;
    ~EditorProfileSettingsBinding();

    EditorProfileSettingsBinding(const EditorProfileSettingsBinding&) = delete;
    EditorProfileSettingsBinding& operator=(const EditorProfileSettingsBinding&) = delete;

    bool Init(EditorProfileRegistry& registry, IEditorSettingsStore& settings);
    void Shutdown();

    [[nodiscard]] bool IsBound() const noexcept;

private:
    EditorProfileRegistry* m_registry = nullptr;
    IEditorSettingsStore* m_settings = nullptr;
    EditorProfileChangeSubscription m_subscription = kInvalidEditorProfileSubscription;
};

} // namespace SagaEditor
