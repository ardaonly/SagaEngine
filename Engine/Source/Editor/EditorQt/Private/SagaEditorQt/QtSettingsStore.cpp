/// @file QtSettingsStore.cpp
/// @brief QSettings-backed implementation of IEditorSettingsStore.

#include "SagaEditorQt/QtSettingsStore.h"

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

} // namespace SagaEditor
