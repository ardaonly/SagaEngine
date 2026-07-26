// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Host/EditorApp.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditorQt/EditorApplicationRunner.h"
#include "SagaEditorQt/QtUIMainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QMainWindow>
#include <QPointer>
#include <QWidget>

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

TEST(QtUIMainWindowTests, OwnsCentralPanelAndAppliesCloseVeto)
{
    auto& application = Application();
    QtUIMainWindow window("Owned Editor", 640, 360);
    ASSERT_NE(window.Window(), nullptr);

    QPointer<QWidget> panel = new QWidget();
    window.DockPanel(panel, "center", "Center", UIDockArea::Center);
    EXPECT_EQ(window.Window()->centralWidget(), panel);

    int closeCalls = 0;
    window.SetOnClose([&closeCalls]
    {
        ++closeCalls;
        return false;
    });
    window.Show();
    application.processEvents();
    window.Close();
    application.processEvents();
    EXPECT_EQ(closeCalls, 1);
    EXPECT_TRUE(window.Window()->isVisible());

    window.UndockPanel("center");
    EXPECT_TRUE(panel.isNull());
    window.SetOnClose([] { return true; });
    window.Close();
    application.processEvents();
    EXPECT_FALSE(window.Window()->isVisible());
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
