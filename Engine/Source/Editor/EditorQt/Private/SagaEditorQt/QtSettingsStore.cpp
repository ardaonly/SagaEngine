// SPDX-License-Identifier: MPL-2.0
/// @file QtSettingsStore.cpp
/// @brief QSettings-backed implementation of IEditorSettingsStore.

#include "SagaEditorQt/QtSettingsStore.h"

#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QVariant>

namespace SagaEditor
{

struct QtSettingsStore::Impl
{
    QSettings settings{ "Saga", "Editor" };
};

QtSettingsStore::QtSettingsStore()
    : m_impl(std::make_unique<Impl>())
{}

QtSettingsStore::~QtSettingsStore() = default;

std::string QtSettingsStore::GetString(const std::string& key,
                                       const std::string& fallback) const
{
    const QVariant value = m_impl->settings.value(QString::fromStdString(key),
                                                  QString::fromStdString(fallback));
    return value.toString().toStdString();
}

void QtSettingsStore::SetString(const std::string& key, const std::string& value)
{
    m_impl->settings.setValue(QString::fromStdString(key),
                              QString::fromStdString(value));
}

void QtSettingsStore::Remove(const std::string& key)
{
    m_impl->settings.remove(QString::fromStdString(key));
}

void QtSettingsStore::Flush()
{
    m_impl->settings.sync();
}

std::optional<std::vector<std::uint8_t>> QtSettingsStore::GetBytes(
    const std::string& key) const
{
    const auto qKey = QString::fromStdString(key);
    if (!m_impl->settings.contains(qKey)) return std::nullopt;
    const auto bytes = m_impl->settings.value(qKey).toByteArray();
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

std::filesystem::path QtSettingsStore::ConfigurationRoot() const
{
    return QFileInfo(m_impl->settings.fileName()).absolutePath().toStdString();
}

} // namespace SagaEditor
