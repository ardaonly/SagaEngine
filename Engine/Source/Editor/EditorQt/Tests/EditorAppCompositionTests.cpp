// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Host/EditorApp.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditorQt/EditorApplicationRunner.h"
#include "SagaEditorQt/QtUIMainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>

#include <memory>
#include <sstream>
#include <string>

namespace SagaEditor {
namespace {

QApplication& Application()
{
    if (auto* existing = qobject_cast<QApplication*>(
            QCoreApplication::instance()))
    {
        return *existing;
    }
    static int argc = 1;
    static char name[] = "editor-app-composition-tests";
    static char* argv[] = {name, nullptr};
    static QApplication application(argc, argv);
    return application;
}

TEST(EditorAppCompositionTests, OwnsTypedSettingsWindowHostAndShell)
{
    (void)Application();

    EditorAppConfig config;
    config.windowTitle = "Composed Editor";
    config.windowWidth = 720;
    config.windowHeight = 480;
    config.showOnInit = false;
    config.registerDefaultPanels = false;
    config.applyActivePersona = false;

    EditorApp app;
    ASSERT_TRUE(app.Init(
        config,
        std::make_unique<MemoryEditorSettingsStore>(),
        std::make_unique<QtUIMainWindow>(
            config.windowTitle, config.windowWidth, config.windowHeight)));

    EXPECT_EQ(app.GetShell().GetMainWindow().GetWidth(), 720);
    EXPECT_EQ(app.GetShell().GetMainWindow().GetHeight(), 480);
    app.Shutdown();
}

TEST(EditorApplicationRunnerTests, HelpAndInvalidArgumentsDoNotRequireQtStartup)
{
    {
        char executable[] = "SagaEditor";
        char help[] = "--help";
        char* argv[] = {executable, help};
        std::ostringstream out;
        std::ostringstream err;
        EXPECT_EQ(RunEditorApplication(2, argv, out, err), 0);
        EXPECT_NE(out.str().find("Usage: SagaEditor"), std::string::npos);
        EXPECT_TRUE(err.str().empty());
    }
    {
        char executable[] = "SagaEditor";
        char invalid[] = "--not-an-editor-option";
        char* argv[] = {executable, invalid};
        std::ostringstream out;
        std::ostringstream err;
        EXPECT_EQ(RunEditorApplication(2, argv, out, err), 2);
        EXPECT_TRUE(out.str().empty());
        EXPECT_NE(err.str().find("unknown argument"), std::string::npos);
    }
}

} // namespace
} // namespace SagaEditor
