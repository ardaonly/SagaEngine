// SPDX-License-Identifier: MPL-2.0
/// @file SagaLauncherUiTests.cpp
/// @brief Offscreen Saga Home UI contracts.

#include "Launcher/SagaMainWindow.h"
#include "SagaEditorQt/QtUIMainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>

#include <filesystem>
#include <memory>

namespace
{

std::filesystem::path TestSourceRoot()
{
    const auto configured = std::filesystem::path(SAGA_SOURCE_ROOT);
    if (std::filesystem::is_regular_file(
            configured / "Samples/StarterArena/StarterArena.sagaproj"))
        return configured;
    auto cursor = std::filesystem::current_path();
    for (;;)
    {
        if (std::filesystem::is_regular_file(
                cursor / "Samples/StarterArena/StarterArena.sagaproj"))
            return cursor;
        const auto parent = cursor.parent_path();
        if (parent == cursor)
            return configured;
        cursor = parent;
    }
}

TEST(SagaHomeUiTest, PermanentHomeActionsAndModesExist)
{
    SagaProduct::SagaMainWindow window(QApplication::applicationFilePath().toStdString());
    QApplication::processEvents();

    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    EXPECT_NE(window.findChild<QWidget*>("SagaHomePage"), nullptr);
    EXPECT_NE(window.findChild<QPushButton*>("SagaHomeNewProject"), nullptr);
    EXPECT_NE(window.findChild<QPushButton*>("SagaHomeOpenProject"), nullptr);
    EXPECT_NE(window.findChild<QListWidget*>("SagaHomeRecentProjects"), nullptr);
    auto* modes = window.findChild<QComboBox*>("SagaHomeAuthoringMode");
    ASSERT_NE(modes, nullptr);
    EXPECT_EQ(modes->count(), 3);
    EXPECT_EQ(modes->itemData(0).toString(), "level1");
    EXPECT_EQ(modes->itemData(1).toString(), "level2");
    EXPECT_EQ(modes->itemData(2).toString(), "level3");
}

TEST(SagaHomeUiTest, RoleRouterAndReportControlsAreAbsent)
{
    SagaProduct::SagaMainWindow window(QApplication::applicationFilePath().toStdString());
    QApplication::processEvents();

    QStringList labels;
    for (const auto* button : window.findChildren<QPushButton*>())
        labels.push_back(button->text());
    const QString joined = labels.join('|');
    EXPECT_FALSE(joined.contains("Open Editor"));
    EXPECT_FALSE(joined.contains("Validate Project"));
    EXPECT_FALSE(joined.contains("Target"));
    EXPECT_FALSE(joined.contains("Report"));
    EXPECT_FALSE(joined.contains("EditorLab"));
}

TEST(SagaHomeUiTest, FailedOpenStaysHomeAndEditorCanUnmountAndRemount)
{
    SagaProduct::SagaMainWindow window(QApplication::applicationFilePath().toStdString());
    ASSERT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    EXPECT_FALSE(window.OpenProject("missing-project.sagaproj"));
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);

    const auto project = TestSourceRoot() /
        "Samples/StarterArena/StarterArena.sagaproj";
    ASSERT_TRUE(window.OpenProject(project));
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Editor);
    EXPECT_EQ(window.windowTitle(), "Saga — StarterArena");
    EXPECT_NE(window.findChild<QDockWidget*>("saga.panel.hierarchy"), nullptr);
    EXPECT_NE(window.findChild<QDockWidget*>("saga.panel.inspector"), nullptr);

    window.ReturnHome();
    QApplication::processEvents();
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    EXPECT_EQ(window.windowTitle(), "Saga");
    EXPECT_TRUE(window.findChildren<QDockWidget*>().empty());

    ASSERT_TRUE(window.OpenProject(project));
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Editor);
    window.ReturnHome();
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
}

TEST(SagaHomeUiTest, AttachedEditorWindowNeverOwnsProductWindow)
{
    auto owner = std::make_unique<QMainWindow>();
    QPointer<QMainWindow> observed(owner.get());
    auto* stack = new QStackedWidget(owner.get());
    owner->setCentralWidget(stack);
    {
        SagaEditor::QtUIMainWindow editorWindow(*owner, *stack);
        editorWindow.DockPanel(new QWidget(), "saga.panel.viewport",
                               "World Viewport", SagaEditor::UIDockArea::Center);
        editorWindow.DockPanel(new QWidget(), "saga.panel.hierarchy",
                               "Hierarchy", SagaEditor::UIDockArea::Left);
        EXPECT_EQ(stack->count(), 1);
        EXPECT_NE(owner->findChild<QDockWidget*>("saga.panel.hierarchy"), nullptr);
    }
    EXPECT_FALSE(observed.isNull());
    EXPECT_EQ(stack->count(), 0);
    EXPECT_TRUE(owner->findChildren<QDockWidget*>().empty());
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    QTemporaryDir settingsRoot;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsRoot.path());
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
