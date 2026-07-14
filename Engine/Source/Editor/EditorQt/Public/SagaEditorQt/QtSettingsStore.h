/// @file QtSettingsStore.h
/// @brief QSettings-backed implementation of IEditorSettingsStore.

#pragma once

#include "SagaEditor/Settings/IEditorSettingsStore.h"

#include <memory>

namespace SagaEditor
{

class QtSettingsStore final : public IEditorSettingsStore
{
public:
    QtSettingsStore();
    ~QtSettingsStore() override;

    [[nodiscard]] std::string GetString(const std::string& key,
                                        const std::string& fallback) const override;

    void SetString(const std::string& key, const std::string& value) override;
    void Remove(const std::string& key) override;
    void Flush() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
