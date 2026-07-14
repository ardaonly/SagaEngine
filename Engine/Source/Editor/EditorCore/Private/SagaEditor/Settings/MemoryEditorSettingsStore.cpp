/// @file MemoryEditorSettingsStore.cpp
/// @brief In-memory settings store used by tests and headless construction.

#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"

namespace SagaEditor
{

std::string MemoryEditorSettingsStore::GetString(const std::string& key,
                                                 const std::string& fallback) const
{
    const auto it = m_values.find(key);
    return it == m_values.end() ? fallback : it->second;
}

void MemoryEditorSettingsStore::SetString(const std::string& key,
                                          const std::string& value)
{
    m_values[key] = value;
}

void MemoryEditorSettingsStore::Remove(const std::string& key)
{
    m_values.erase(key);
}

void MemoryEditorSettingsStore::Flush()
{
}

bool MemoryEditorSettingsStore::Has(const std::string& key) const noexcept
{
    return m_values.find(key) != m_values.end();
}

} // namespace SagaEditor
