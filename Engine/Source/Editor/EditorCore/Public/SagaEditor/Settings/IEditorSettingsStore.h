/// @file IEditorSettingsStore.h
/// @brief Framework-free key/value store for private editor preferences.

#pragma once

#include <string>

namespace SagaEditor
{

/// Stores user-private editor preferences such as active persona and
/// workspace ergonomics. Implementations decide where values live
/// (QSettings for Qt, memory for tests) while editor core stays backend-free.
class IEditorSettingsStore
{
public:
    virtual ~IEditorSettingsStore() = default;

    [[nodiscard]] virtual std::string GetString(const std::string& key,
                                                const std::string& fallback) const = 0;

    virtual void SetString(const std::string& key, const std::string& value) = 0;
    virtual void Remove(const std::string& key) = 0;
    virtual void Flush() = 0;
};

} // namespace SagaEditor
