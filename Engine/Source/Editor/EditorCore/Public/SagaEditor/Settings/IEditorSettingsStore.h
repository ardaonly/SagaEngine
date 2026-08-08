// SPDX-License-Identifier: MPL-2.0
/// @file IEditorSettingsStore.h
/// @brief Framework-free key/value store for private editor preferences.

#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
    /// Returns the backend's exact stored bytes. This is used only for legacy
    /// opaque placement state where text conversion would corrupt Qt state.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> GetBytes(
        const std::string& key) const
    {
        const std::string missing = "\x1f" "saga-missing-setting" "\x1f";
        const auto value = GetString(key, missing);
        if (value == missing) return std::nullopt;
        return std::vector<std::uint8_t>(value.begin(), value.end());
    }
    [[nodiscard]] virtual std::filesystem::path ConfigurationRoot() const
    { return {}; }
};

} // namespace SagaEditor
