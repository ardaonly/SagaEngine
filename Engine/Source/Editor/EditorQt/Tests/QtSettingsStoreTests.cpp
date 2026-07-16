// SPDX-License-Identifier: MPL-2.0

#include "SagaEditorQt/QtSettingsStore.h"

#include <gtest/gtest.h>

#include <QSettings>
#include <QTemporaryDir>

namespace SagaEditor {
namespace {

TEST(QtSettingsStoreTests, FlushPersistsValuesAndRemoveRestoresFallback)
{
    QTemporaryDir settingsDirectory;
    ASSERT_TRUE(settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    // The production store uses Qt's organization/application constructor.
    // Isolate both formats because Qt resolves that constructor through the
    // platform default before applying the test-selected default on some
    // supported Qt builds.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       settingsDirectory.path());

    constexpr const char* key = "tests/qt-settings-store/round-trip";
    {
        QtSettingsStore writer;
        writer.Remove(key);
        writer.SetString(key, "saved-value");
        writer.Flush();
    }

    {
        QtSettingsStore reader;
        EXPECT_EQ(reader.GetString(key, "fallback"), "saved-value");
        reader.Remove(key);
        reader.Flush();
    }

    QtSettingsStore emptyStore;
    EXPECT_EQ(emptyStore.GetString(key, "fallback"), "fallback");
}

} // namespace
} // namespace SagaEditor
