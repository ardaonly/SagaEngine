// SPDX-License-Identifier: MPL-2.0
/// @file SagaLauncherUiTests.cpp
/// @brief Offscreen Saga Home UI contracts.

#include "Launcher/SagaMainWindow.h"
#include "SagaEditor/Project/ProjectMutation.h"
#include "SagaEditorQt/QtUIMainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QListWidget>
#include <QMainWindow>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <thread>

namespace
{

constexpr const char* kWorkflowStateTimerObjectName =
    "SagaEditorWorkflowStateTimer";

class CloseOwningMainWindow final : public QMainWindow
{
public:
    int closeEventCount = 0;

protected:
    void closeEvent(QCloseEvent* event) override
    {
        ++closeEventCount;
        event->ignore();
    }
};

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

bool WaitUntil(const std::function<bool()>& predicate, int timeoutMs = 30000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs)
    {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

QPushButton* FindButton(QWidget* root, const QString& text)
{
    for (auto* button : root->findChildren<QPushButton*>())
        if (button->text() == text) return button;
    return nullptr;
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

TEST(SagaHomeUiTest, NewProjectUsesCanonicalWorkflowAndOpensEditor)
{
    QTemporaryDir parent;
    ASSERT_TRUE(parent.isValid());
    const auto destination =
        std::filesystem::path(parent.path().toStdString()) / "CreatedFromHome";
    ASSERT_FALSE(std::filesystem::exists(destination));
    SagaProduct::SagaMainWindow window(
        QApplication::applicationFilePath().toStdString());

    const auto result =
        window.CreateProject(destination, "Created From Home");

    ASSERT_TRUE(result.passed) <<
        (result.diagnostics.empty() ? std::string{} :
         result.diagnostics.front().code + ": " +
         result.diagnostics.front().message);
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Editor);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        result.project.manifestPath));
    EXPECT_EQ(result.project.manifestPath.parent_path(), destination);
    EXPECT_NE(window.findChild<QDockWidget*>("saga.panel.hierarchy"), nullptr);
    EXPECT_NE(window.findChild<QDockWidget*>("saga.panel.inspector"), nullptr);

    window.ReturnHome();
    QApplication::processEvents();
    ASSERT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    auto* recents = window.findChild<QListWidget*>("SagaHomeRecentProjects");
    ASSERT_NE(recents, nullptr);
    bool found = false;
    for (int index = 0; index < recents->count(); ++index)
        found = found || recents->item(index)->data(Qt::UserRole).toString() ==
            QString::fromStdString(result.project.manifestPath.string());
    EXPECT_TRUE(found);
}

TEST(SagaHomeUiTest, ExistingDestinationIsRejectedWithoutOverwrite)
{
    QTemporaryDir parent;
    ASSERT_TRUE(parent.isValid());
    const auto destination =
        std::filesystem::path(parent.path().toStdString()) / "Existing";
    std::filesystem::create_directories(destination);
    const auto sentinel = destination / "keep.txt";
    std::ofstream(sentinel) << "do not overwrite\n";
    SagaProduct::SagaMainWindow window(
        QApplication::applicationFilePath().toStdString());

    const auto result = window.CreateProject(destination, "Existing");

    ASSERT_FALSE(result.passed);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "Project.Template.CreateFailed");
    EXPECT_FALSE(result.diagnostics.front().message.empty());
    EXPECT_EQ(window.State(), SagaProduct::SagaMainWindowState::Home);
    std::ifstream input(sentinel);
    std::string contents;
    std::getline(input, contents);
    EXPECT_EQ(contents, "do not overwrite");
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

TEST(SagaHomeUiTest, AttachedWorkflowTimerDoesNotOutliveEditorAdapter)
{
    QMainWindow owner;
    auto* stack = new QStackedWidget(&owner);
    owner.setCentralWidget(stack);
    const auto timerCount = [&owner]
    {
        return owner.findChildren<QTimer*>(kWorkflowStateTimerObjectName).size();
    };
    const qsizetype baseline = timerCount();

    for (int cycle = 0; cycle < 3; ++cycle)
    {
        int callbackCount = 0;
        {
            SagaEditor::QtUIMainWindow editorWindow(owner, *stack);
            editorWindow.SetOnWorkflowStatePoll(
                [&callbackCount] { ++callbackCount; });
            const auto timers =
                owner.findChildren<QTimer*>(kWorkflowStateTimerObjectName);
            ASSERT_EQ(timers.size(), baseline + 1);
            ASSERT_TRUE(QMetaObject::invokeMethod(
                timers.back(), "timeout", Qt::QueuedConnection));
        }

        QApplication::processEvents();
        EXPECT_EQ(callbackCount, 0);
        EXPECT_EQ(timerCount(), baseline);
    }
}

TEST(SagaHomeUiTest, AttachedEditorLeavesClosePolicyWithProductWindow)
{
    CloseOwningMainWindow owner;
    auto* stack = new QStackedWidget(&owner);
    owner.setCentralWidget(stack);
    int editorCloseCallbackCount = 0;
    SagaEditor::QtUIMainWindow editorWindow(owner, *stack);
    editorWindow.SetOnClose([&editorCloseCallbackCount]
    {
        ++editorCloseCallbackCount;
        return true;
    });

    owner.close();

    EXPECT_EQ(owner.closeEventCount, 1);
    EXPECT_EQ(editorCloseCallbackCount, 0);
}

TEST(SagaHomeUiTest, XcbPackagesWorkflowRendersProductionPanelReviewAndRecovery)
{
    if (QApplication::platformName() != QStringLiteral("xcb"))
        GTEST_SKIP() << "This rendered product integration requires Qt xcb.";
    const auto evidenceValue = qgetenv("SAGA_XCB_EVIDENCE_DIR");
    ASSERT_FALSE(evidenceValue.isEmpty());
    const auto evidence = std::filesystem::path(evidenceValue.toStdString());
    std::filesystem::create_directories(evidence);

    QTemporaryDir materialization;
    ASSERT_TRUE(materialization.isValid());
    const auto projectRoot = std::filesystem::path(
        materialization.path().toStdString()) / "ArenaRush";
    std::filesystem::copy(TestSourceRoot() / "Samples/ArenaRush", projectRoot,
        std::filesystem::copy_options::recursive);
    const auto project = projectRoot / "ArenaRush.sagaproj";

    {
        SagaProduct::SagaMainWindow window(
            QApplication::applicationFilePath().toStdString());
        ASSERT_TRUE(window.OpenProject(project));
        window.show();
        auto* dock = window.findChild<QDockWidget*>("saga.panel.packages");
        ASSERT_NE(dock, nullptr);
        dock->show();
        auto* tree = dock->findChild<QTreeWidget*>();
        ASSERT_NE(tree, nullptr);
        ASSERT_TRUE(WaitUntil([tree] { return tree->topLevelItemCount() > 0; }));
        ASSERT_TRUE(window.grab().save(QString::fromStdString(
            (evidence / "packages-window.png").string())));
        ASSERT_TRUE(dock->grab().save(QString::fromStdString(
            (evidence / "packages-panel.png").string())));

        auto* codeDock = window.findChild<QDockWidget*>("saga.panel.code_box");
        ASSERT_NE(codeDock, nullptr);
        codeDock->show();
        auto* selector = codeDock->findChild<QComboBox*>();
        ASSERT_NE(selector, nullptr);
        const auto gameModeIndex = selector->findText(
            QStringLiteral("ArenaRushGameMode.cs"));
        ASSERT_GE(gameModeIndex, 0);
        selector->setCurrentIndex(gameModeIndex);
        auto* analyze = FindButton(codeDock, QStringLiteral("Analyze"));
        ASSERT_NE(analyze, nullptr);
        analyze->click();
        QWidget* energyCore = nullptr;
        ASSERT_TRUE(WaitUntil([codeDock, &energyCore]
        {
            for (auto* candidate : codeDock->findChildren<QWidget*>())
                if (candidate->accessibleName() ==
                    QStringLiteral("Energy Core controlled fields"))
                {
                    energyCore = candidate;
                    break;
                }
            return energyCore &&
                energyCore->findChild<QSpinBox*>()->isEnabled() &&
                energyCore->findChild<QDoubleSpinBox*>()->isEnabled();
        }));
        EXPECT_EQ(energyCore->findChild<QSpinBox*>()->value(), 10);
        EXPECT_DOUBLE_EQ(energyCore->findChild<QDoubleSpinBox*>()->value(), 2.0);
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "blocks-controlled-fields.png").string())));

        QTreeWidgetItem* instrumentation = nullptr;
        for (int row = 0; row < tree->topLevelItemCount(); ++row)
            if (tree->topLevelItem(row)->text(2) == "collectibles.instrumentation")
                instrumentation = tree->topLevelItem(row);
        ASSERT_NE(instrumentation, nullptr);
        tree->setCurrentItem(instrumentation);
        instrumentation->setSelected(true);
        QApplication::processEvents();
        auto* enable = FindButton(dock, QStringLiteral("Enable module…"));
        ASSERT_NE(enable, nullptr);
        ASSERT_TRUE(WaitUntil([enable] { return enable->isEnabled(); }));
        enable->click();
        QDialog* review = nullptr;
        ASSERT_TRUE(WaitUntil([&window, &review]
        {
            review = window.findChild<QDialog*>("PackageReviewDialog");
            return review != nullptr;
        }));
        ASSERT_TRUE(review->grab().save(QString::fromStdString(
            (evidence / "package-review.png").string())));
        review->reject();
        ASSERT_TRUE(WaitUntil([&window]
        { return window.findChild<QDialog*>("PackageReviewDialog") == nullptr; }));
        window.ReturnHome();
    }

    const auto lockPath = projectRoot / "SagaPackage.lock";
    std::ifstream lockInput(lockPath, std::ios::binary);
    const std::string lockBytes{std::istreambuf_iterator<char>(lockInput), {}};
    {
        auto [lease, acquired] = SagaEditor::ProjectMutationLease::Acquire(
            projectRoot, "xcb-recovery-fixture");
        ASSERT_TRUE(acquired.succeeded) << acquired.code << ": " << acquired.message;
        SagaEditor::ProjectMutationTransaction transaction(
            lease, "xcb-recovery-fixture", "package-workflow-v1");
        ASSERT_TRUE(transaction.Stage({{"SagaPackage.lock",
            lockBytes + "\n"}}).succeeded);
        ASSERT_TRUE(transaction.MarkCommit().succeeded);
    }
    std::ofstream(lockPath, std::ios::binary | std::ios::app) << "divergence\n";

    SagaProduct::SagaMainWindow recoveryWindow(
        QApplication::applicationFilePath().toStdString());
    ASSERT_TRUE(recoveryWindow.OpenProject(project));
    recoveryWindow.show();
    auto* dock = recoveryWindow.findChild<QDockWidget*>("saga.panel.packages");
    ASSERT_NE(dock, nullptr);
    dock->show();
    auto* recover = FindButton(dock,
        QStringLiteral("Recover Transaction…"));
    ASSERT_NE(recover, nullptr);
    ASSERT_TRUE(WaitUntil([recover] { return recover->isEnabled(); }));
    recover->click();
    QDialog* recovery = nullptr;
    ASSERT_TRUE(WaitUntil([&recoveryWindow, &recovery]
    {
        recovery = recoveryWindow.findChild<QDialog*>("PackageRecoveryDialog");
        return recovery != nullptr;
    }));
    ASSERT_TRUE(recovery->grab().save(QString::fromStdString(
        (evidence / "package-recovery.png").string())));
    recovery->reject();
    recoveryWindow.ReturnHome();
}

} // namespace

int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    QTemporaryDir settingsRoot;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsRoot.path());
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
