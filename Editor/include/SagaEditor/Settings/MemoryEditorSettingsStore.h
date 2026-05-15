/// @file MemoryEditorSettingsStore.h
/// @brief In-memory settings store used by tests and headless construction.

#pragma once

#include "SagaEditor/Settings/IEditorSettingsStore.h"

#include <string>
#include <unordered_map>

namespace SagaEditor
{

class MemoryEditorSettingsStore final : public IEditorSettingsStore
{
public:
    MemoryEditorSettingsStore() = default;
    ~MemoryEditorSettingsStore() override = default;

    [[nodiscard]] std::string GetString(const std::string& key,
                                        const std::string& fallback) const override;

    void SetString(const std::string& key, const std::string& value) override;
    void Remove(const std::string& key) override;
    void Flush() override;

    [[nodiscard]] bool Has(const std::string& key) const noexcept;

private:
    std::unordered_map<std::string, std::string> m_values;
};

} // namespace SagaEditor
