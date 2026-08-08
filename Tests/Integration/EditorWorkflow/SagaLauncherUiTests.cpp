// SPDX-License-Identifier: MPL-2.0
/// @file SagaLauncherUiTests.cpp
/// @brief Offscreen Saga Home UI contracts.

#include "Launcher/SagaMainWindow.h"
#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Play/PlayPreparationPipeline.h"
#include "SagaEditor/Project/ProjectMutation.h"
#include "SagaEditor/Processes/ToolProcessRunner.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditorQt/QtUIMainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScreen>
#include <QSettings>
#include <QStackedWidget>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <thread>

#ifndef _WIN32
#include <csignal>
#include <unistd.h>
#endif
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
bool ListContains(QListWidget* list, std::initializer_list<const char*> fragments)
{
    for (int index = 0; index < list->count(); ++index)
        for (const auto* fragment : fragments)
            if (list->item(index)->text().contains(QString::fromLatin1(fragment))) return true;
    return false;
}

QPushButton* FindButton(QWidget* root, const QString& text)
{
    for (auto* button : root->findChildren<QPushButton*>())
        if (button->text() == text) return button;
    return nullptr;
}

QAction* FindCommand(QWidget* root, const QString& commandId)
{
    for (auto* action : root->findChildren<QAction*>())
        if (action->data().toString() == commandId) return action;
    return nullptr;
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

std::uint64_t LanguageServiceProcess(std::uint64_t excluded = 0)
{
#ifdef _WIN32
    (void)excluded;
    return 0;
#else
    const auto parent = static_cast<std::uint64_t>(::getpid());
    for (const auto& entry : std::filesystem::directory_iterator("/proc"))
    {
        const auto name = entry.path().filename().string();
        if (name.empty() || !std::all_of(name.begin(), name.end(), ::isdigit))
            continue;
        const auto pid = std::stoull(name);
        if (pid == excluded) continue;
        std::ifstream status(entry.path() / "status");
        std::string line;
        std::uint64_t processParent = 0;
        while (std::getline(status, line))
            if (line.rfind("PPid:", 0) == 0)
            {
                processParent = std::stoull(line.substr(5));
                break;
            }
        if (processParent != parent) continue;
        std::ifstream commandInput(entry.path() / "cmdline", std::ios::binary);
        std::string command{std::istreambuf_iterator<char>(commandInput), {}};
        std::replace(command.begin(), command.end(), '\0', ' ');
        if (command.find("sagascript") != std::string::npos &&
            command.find("language-service") != std::string::npos)
            return pid;
    }
    return 0;
#endif
}

std::filesystem::path RuntimeExecutable()
{
    return std::filesystem::path(SAGA_BUILD_ROOT) / "bin/SagaRuntime";
}

nlohmann::json RunAuthoredArenaRush(const std::filesystem::path& root,
                                    std::string session,
                                    bool singleCollect)
{
    {
        const auto scenePath = root / "Scenes/ArenaRush.sagascene";
        std::ifstream input(scenePath, std::ios::binary);
        auto scene = nlohmann::json::parse(input);
        bool configured = false;
        for (auto& entity : scene["entities"])
            for (auto& component : entity["components"])
                if (component.value("type", std::string{}) == "ScriptBehavior" &&
                    component.value("behaviorTypeId", std::string{}) ==
                        "ArenaRush.ArenaRushGameMode")
                {
                    component["instanceConfiguration"]["targetProgress"] =
                        singleCollect ? 20 : 200;
                    configured = true;
                }
        EXPECT_TRUE(configured);
        std::ofstream(scenePath, std::ios::binary | std::ios::trunc)
            << scene.dump(2) << '\n';
    }

    SagaEditor::EditorDocumentWorkspace workspace;
    EXPECT_TRUE(workspace.OpenProject(root / "ArenaRush.sagaproj"));
    const auto prepared = SagaEditor::PlayPreparationPipeline{}.Prepare(
        workspace, {std::move(session), RuntimeExecutable(), {}, {}, {}});
    EXPECT_TRUE(prepared.passed) <<
        (prepared.diagnostics.empty() ? std::string{} :
         prepared.diagnostics.front().code + ": " +
         prepared.diagnostics.front().message);
    if (!prepared.passed) return {};

    SagaEditor::ToolProcessRequest request;
    request.executable = RuntimeExecutable();
    request.arguments = {"--play-envelope", prepared.localEnvelopePath.string(),
                         "--headless", "--smoke-interactions"};
    request.workingDirectory = prepared.buildRoot;
    request.inheritEnvironment = false;
    {
        std::ifstream envelopeInput(prepared.localEnvelopePath, std::ios::binary);
        const auto envelope = nlohmann::json::parse(envelopeInput);
        if (envelope.contains("environment") && envelope["environment"].is_object())
            for (const auto& [name, value] : envelope["environment"].items())
                if (value.is_string())
                    request.environment[name] = value.get<std::string>();
        for (const char* name : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                                 "XDG_RUNTIME_DIR", "DOTNET_ROOT",
                                 "LD_LIBRARY_PATH"})
            if (const char* value = std::getenv(name); value && *value)
                request.environment[name] = value;
        const auto managedRoot = std::filesystem::path(SAGA_BUILD_ROOT) /
            "ManagedToolRuntime/runtime/dotnet";
        request.environment["DOTNET_ROOT"] = managedRoot.string();
        request.environment["DOTNET_ROOT_X64"] = managedRoot.string();
        request.environment["DOTNET_SYSTEM_GLOBALIZATION_INVARIANT"] = "1";
    }
    request.timeout = std::chrono::seconds(60);
    const auto process = SagaEditor::ToolProcessRunner{}.Run(request);
    EXPECT_TRUE(process.Succeeded()) << process.diagnosticCode << ": "
        << process.diagnosticMessage << "\nexit=" << process.exitCode
        << "\nstdout=" << process.standardOutput
        << "\nstderr=" << process.standardError;
    if (!process.Succeeded() ||
        !std::filesystem::is_regular_file(prepared.resultPath)) return {};
    std::ifstream result(prepared.resultPath, std::ios::binary);
    return nlohmann::json::parse(result);
}

void SetCursorAfter(QPlainTextEdit* editor, const QString& text, int occurrence = 0)
{
    auto cursor = editor->document()->find(text);
    while (occurrence-- > 0 && !cursor.isNull())
        cursor = editor->document()->find(text, cursor);
    ASSERT_FALSE(cursor.isNull());
    cursor.clearSelection();
    editor->setTextCursor(cursor);
    editor->setFocus();
}

void SendKey(QWidget* target, int key, Qt::KeyboardModifiers modifiers = {})
{
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(target, &release);
    QApplication::processEvents();
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
        auto* tabs = codeDock->findChild<QTabWidget*>();
        ASSERT_NE(tabs, nullptr);
        ASSERT_EQ(tabs->count(), 2);
        EXPECT_EQ(tabs->tabText(1),
            QStringLiteral("Blocks - Controlled C# authoring"));
        tabs->setCurrentIndex(1);
        QApplication::processEvents();
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "blocks-controlled-fields.png").string())));
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "editable-controls.png").string())));

        auto* editor = codeDock->findChild<QPlainTextEdit*>(
            QStringLiteral("CodeBoxCSharpEditor"));
        QListWidget* languageResults = nullptr;
        for (auto* candidate : codeDock->findChildren<QListWidget*>())
            if (candidate->accessibleName() ==
                QStringLiteral("Code Box language results"))
                languageResults = candidate;
        auto* resultTitle = [&]() -> QLabel*
        {
            for (auto* candidate : codeDock->findChildren<QLabel*>())
                if (candidate->accessibleName() ==
                    QStringLiteral("Code Box result type")) return candidate;
            return nullptr;
        }();
        auto* diagnosticList = [&]() -> QListWidget*
        {
            for (auto* candidate : codeDock->findChildren<QListWidget*>())
                if (candidate->accessibleName() ==
                    QStringLiteral("SagaScript diagnostics")) return candidate;
            return nullptr;
        }();
        ASSERT_NE(editor, nullptr);
        ASSERT_NE(languageResults, nullptr);
        ASSERT_NE(resultTitle, nullptr);
        ASSERT_NE(diagnosticList, nullptr);
        for (int index = 0; index < diagnosticList->count(); ++index)
        {
            EXPECT_FALSE(diagnosticList->item(index)->text().contains(
                QStringLiteral("CS0246")));
            EXPECT_FALSE(diagnosticList->item(index)->text().contains(
                QStringLiteral("CS0103")));
        }
        const auto originalSource = editor->toPlainText();
        tabs->setCurrentIndex(0);

        SetCursorAfter(editor, QStringLiteral("EnergyCoreScore"), 1);
        ASSERT_TRUE(WaitUntil([resultTitle, languageResults]
        {
            return resultTitle->text() == QStringLiteral("Hover information") &&
                languageResults->count() > 0;
        }));
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "hover.png").string())));

        FindButton(codeDock, QStringLiteral("Definition"))->click();
        ASSERT_TRUE(WaitUntil([resultTitle, languageResults]
        {
            return resultTitle->text() == QStringLiteral("Definition results") &&
                languageResults->count() > 0;
        }));
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "definition-results.png").string())));
        FindButton(codeDock, QStringLiteral("References"))->click();
        ASSERT_TRUE(WaitUntil([resultTitle, languageResults]
        {
            return resultTitle->text() == QStringLiteral("Reference results") &&
                languageResults->count() > 0;
        }));
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "references-results.png").string())));
        FindButton(codeDock, QStringLiteral("Symbols"))->click();
        ASSERT_TRUE(WaitUntil([resultTitle, languageResults]
        {
            return resultTitle->text() == QStringLiteral("Symbol results") &&
                languageResults->count() > 0;
        }));
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "symbols-results.png").string())));

        SetCursorAfter(editor, QStringLiteral("cores."));
        SendKey(editor, Qt::Key_Space, Qt::ControlModifier);
        QWidget* completionPopup = nullptr;
        ASSERT_TRUE(WaitUntil([&completionPopup]
        {
            for (auto* candidate : QApplication::topLevelWidgets())
                if (candidate->accessibleName() ==
                    QStringLiteral("Code Box completion popup") &&
                    candidate->isVisible())
                {
                    completionPopup = candidate;
                    return true;
                }
            return false;
        }));
        ASSERT_TRUE(window.screen()->grabWindow(0).save(QString::fromStdString(
            (evidence / "completion-context.png").string())));
        ASSERT_TRUE(completionPopup->grab().save(QString::fromStdString(
            (evidence / "completion-popup.png").string())));
        SendKey(editor, Qt::Key_Escape);

        editor->setPlainText(originalSource +
            QStringLiteral("\ninvalid diagnostic token\n"));
        ASSERT_TRUE(WaitUntil([diagnosticList]
        { return ListContains(diagnosticList, {"CS1002", "CS0116"}); }));
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "diagnostic-marker.png").string())));
        editor->setPlainText(originalSource);
        ASSERT_TRUE(WaitUntil([energyCore, diagnosticList]
        {
            if (!energyCore->findChild<QSpinBox*>()->isEnabled() ||
                !energyCore->findChild<QDoubleSpinBox*>()->isEnabled()) return false;
            return !ListContains(diagnosticList,
                {"CS0246", "CS1002", "CS0116"});
        }));

        tabs->setCurrentIndex(1);
        auto* score = energyCore->findChild<QSpinBox*>();
        auto* respawn = energyCore->findChild<QDoubleSpinBox*>();
        auto* previewScore = FindButton(energyCore,
            QStringLiteral("Preview score change"));
        auto* previewRespawn = FindButton(energyCore,
            QStringLiteral("Preview respawn change"));
        ASSERT_NE(previewScore, nullptr);
        ASSERT_NE(previewRespawn, nullptr);
        score->setValue(20);
        previewScore->click();
        QDialog* scoreReview = nullptr;
        ASSERT_TRUE(WaitUntil([&window, &scoreReview]
        {
            scoreReview = window.findChild<QDialog*>(
                QStringLiteral("ControlledEditReviewDialog"));
            return scoreReview != nullptr;
        }));
        ASSERT_TRUE(scoreReview->grab().save(QString::fromStdString(
            (evidence / "score-diff-review.png").string())));
        scoreReview->findChild<QDialogButtonBox*>()->
            button(QDialogButtonBox::Apply)->click();
        ASSERT_TRUE(WaitUntil([editor]
        { return editor->toPlainText().contains("EnergyCoreScore = 20"); }));
        tabs->setCurrentIndex(0);
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "code-box-after-apply.png").string())));
        tabs->setCurrentIndex(1);
        ASSERT_TRUE(WaitUntil([score]
        { return score->isEnabled() && score->value() == 20; }));
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "blocks-after-apply.png").string())));

        respawn->setValue(0.5);
        previewRespawn->click();
        QDialog* respawnReview = nullptr;
        ASSERT_TRUE(WaitUntil([&window, &respawnReview]
        {
            respawnReview = window.findChild<QDialog*>(
                QStringLiteral("ControlledEditReviewDialog"));
            return respawnReview != nullptr;
        }));
        ASSERT_TRUE(respawnReview->grab().save(QString::fromStdString(
            (evidence / "respawn-diff-review.png").string())));
        respawnReview->findChild<QDialogButtonBox*>()->
            button(QDialogButtonBox::Apply)->click();
        ASSERT_TRUE(WaitUntil([respawn]
        { return respawn->isEnabled() && respawn->value() == 0.5; }));

        score->setValue(30);
        previewScore->click();
        QDialog* conflictReview = nullptr;
        ASSERT_TRUE(WaitUntil([&window, &conflictReview]
        {
            conflictReview = window.findChild<QDialog*>(
                QStringLiteral("ControlledEditReviewDialog"));
            return conflictReview != nullptr;
        }));
        editor->setPlainText(editor->toPlainText() +
            QStringLiteral("\n// conflict\n"));
        conflictReview->findChild<QDialogButtonBox*>()->
            button(QDialogButtonBox::Apply)->click();
        tabs->setCurrentIndex(1);
        ASSERT_TRUE(codeDock->grab().save(QString::fromStdString(
            (evidence / "stale-conflict.png").string())));

        tabs->setCurrentIndex(0);
        editor->setPlainText(originalSource);
        auto* saveCode = FindButton(codeDock, QStringLiteral("Save"));
        ASSERT_NE(saveCode, nullptr);
        saveCode->click();
        ASSERT_TRUE(WaitUntil([editor]
        { return editor->toPlainText().contains("EnergyCoreScore = 10") &&
            editor->toPlainText().contains("EnergyCoreRespawnSeconds = 2.0"); }));

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

TEST(SagaHomeUiTest,
     XcbControlledBlocksSurviveRecoveryAndDriveSavedRuntime)
{
    if (QApplication::platformName() != QStringLiteral("xcb"))
        GTEST_SKIP() << "This product integration requires Qt xcb.";
#ifdef _WIN32
    GTEST_SKIP() << "The worker crash proof requires procfs signals.";
#endif
    QTemporaryDir materialization;
    ASSERT_TRUE(materialization.isValid());
    const auto projectRoot = std::filesystem::path(
        materialization.path().toStdString()) / "ArenaRush";
    std::filesystem::copy(TestSourceRoot() / "Samples/ArenaRush", projectRoot,
        std::filesystem::copy_options::recursive);
    const auto project = projectRoot / "ArenaRush.sagaproj";
    const auto scriptPath = projectRoot / "Scripts/ArenaRushGameMode.cs";
    const auto checkedInSource = ReadTextFile(
        TestSourceRoot() / "Samples/ArenaRush/Scripts/ArenaRushGameMode.cs");

    SagaProduct::SagaMainWindow window(
        QApplication::applicationFilePath().toStdString());
    ASSERT_TRUE(window.OpenProject(project));
    window.show();
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
                energyCore = candidate;
        return energyCore && energyCore->findChild<QSpinBox*>()->isEnabled() &&
            energyCore->findChild<QDoubleSpinBox*>()->isEnabled();
    }));
    auto* editor = codeDock->findChild<QPlainTextEdit*>(
        QStringLiteral("CodeBoxCSharpEditor"));
    auto* score = energyCore->findChild<QSpinBox*>();
    auto* respawn = energyCore->findChild<QDoubleSpinBox*>();
    QLabel* blocksStatus = nullptr;
    for (auto* candidate : codeDock->findChildren<QLabel*>())
        if (candidate->accessibleName() == QStringLiteral("Blocks projection status"))
            blocksStatus = candidate;
    ASSERT_NE(editor, nullptr);
    ASSERT_NE(blocksStatus, nullptr);

    const auto apply = [&](QAbstractSpinBox* control, double value,
                           const QString& previewText)
    {
        if (auto* integer = qobject_cast<QSpinBox*>(control))
            integer->setValue(static_cast<int>(value));
        else
            qobject_cast<QDoubleSpinBox*>(control)->setValue(value);
        auto* preview = FindButton(energyCore, previewText);
        ASSERT_NE(preview, nullptr);
        preview->click();
        QDialog* review = nullptr;
        ASSERT_TRUE(WaitUntil([&window, &review]
        {
            review = window.findChild<QDialog*>(
                QStringLiteral("ControlledEditReviewDialog"));
            return review != nullptr;
        }));
        auto* buttons = review->findChild<QDialogButtonBox*>();
        ASSERT_NE(buttons, nullptr);
        buttons->button(QDialogButtonBox::Apply)->click();
    };

    apply(score, 20, QStringLiteral("Preview score change"));
    ASSERT_TRUE(WaitUntil([editor, score]
    {
        return editor->toPlainText().contains("EnergyCoreScore = 20") &&
            score->isEnabled() && score->value() == 20;
    }));
    apply(respawn, 0.5, QStringLiteral("Preview respawn change"));
    ASSERT_TRUE(WaitUntil([editor, respawn]
    {
        return editor->toPlainText().contains(
            "EnergyCoreRespawnSeconds = 0.5") &&
            respawn->isEnabled() && respawn->value() == 0.5;
    }));

    auto* undo = FindCommand(&window, QStringLiteral("saga.command.edit.undo"));
    auto* redo = FindCommand(&window, QStringLiteral("saga.command.edit.redo"));
    ASSERT_NE(undo, nullptr);
    ASSERT_NE(redo, nullptr);
    ASSERT_TRUE(WaitUntil([undo] { return undo->isEnabled(); }));
    undo->trigger();
    ASSERT_TRUE(WaitUntil([respawn]
    { return respawn->isEnabled() && respawn->value() == 2.0; }));
    ASSERT_TRUE(WaitUntil([undo] { return undo->isEnabled(); }));
    undo->trigger();
    ASSERT_TRUE(WaitUntil([editor, score]
    {
        return editor->toPlainText().contains("EnergyCoreScore = 10") &&
            score->isEnabled() && score->value() == 10;
    })) << editor->toPlainText().toStdString() << "\nscore=" << score->value()
        << "\nstatus=" << blocksStatus->text().toStdString();
    ASSERT_TRUE(WaitUntil([redo] { return redo->isEnabled(); }));
    redo->trigger();
    ASSERT_TRUE(WaitUntil([score]
    { return score->isEnabled() && score->value() == 20; }));
    ASSERT_TRUE(WaitUntil([redo] { return redo->isEnabled(); }));
    redo->trigger();
    ASSERT_TRUE(WaitUntil([respawn]
    { return respawn->isEnabled() && respawn->value() == 0.5; }));

    std::uint64_t worker = 0;
    ASSERT_TRUE(WaitUntil([&worker]
    {
        worker = LanguageServiceProcess();
        return worker != 0;
    }));
#ifndef _WIN32
    ASSERT_EQ(::kill(static_cast<pid_t>(worker), SIGKILL), 0);
#endif
    std::uint64_t recoveredWorker = 0;
    ASSERT_TRUE(WaitUntil([worker, &recoveredWorker]
    {
        recoveredWorker = LanguageServiceProcess(worker);
        return recoveredWorker != 0;
    }, 45000));
    ASSERT_NE(recoveredWorker, worker);
    ASSERT_TRUE(WaitUntil([editor, score, respawn]
    {
        return editor->toPlainText().contains("EnergyCoreScore = 20") &&
            editor->toPlainText().contains(
                "EnergyCoreRespawnSeconds = 0.5") &&
            score->isEnabled() && score->value() == 20 &&
            respawn->isEnabled() && respawn->value() == 0.5;
    }, 45000));

    auto* save = FindButton(codeDock, QStringLiteral("Save"));
    ASSERT_NE(save, nullptr);
    save->click();
    ASSERT_TRUE(WaitUntil([&]
    {
        const auto text = ReadTextFile(scriptPath);
        return text.find("EnergyCoreScore = 20") != std::string::npos &&
            text.find("EnergyCoreRespawnSeconds = 0.5") != std::string::npos;
    }));
    const auto authoredSource = ReadTextFile(scriptPath);
    const auto authoredHash = SagaEditor::HashDocumentBytes(authoredSource);
    ASSERT_NE(authoredSource, checkedInSource);

    const auto scoreRoot = std::filesystem::path(
        materialization.path().toStdString()) / "ScoreRuntime";
    const auto respawnRoot = std::filesystem::path(
        materialization.path().toStdString()) / "RespawnRuntime";
    std::filesystem::copy(projectRoot, scoreRoot,
        std::filesystem::copy_options::recursive);
    std::filesystem::copy(projectRoot, respawnRoot,
        std::filesystem::copy_options::recursive);
    std::error_code cleanupError;
    std::filesystem::remove_all(scoreRoot / "Build/Editor", cleanupError);
    cleanupError.clear();
    std::filesystem::remove_all(respawnRoot / "Build/Editor", cleanupError);
    ASSERT_EQ(SagaEditor::HashDocumentBytes(ReadTextFile(
        scoreRoot / "Scripts/ArenaRushGameMode.cs")), authoredHash);
    ASSERT_EQ(SagaEditor::HashDocumentBytes(ReadTextFile(
        respawnRoot / "Scripts/ArenaRushGameMode.cs")), authoredHash);

    const auto scoreResult = RunAuthoredArenaRush(
        scoreRoot, "blocks-score-runtime", true);
    ASSERT_FALSE(scoreResult.empty());
    ASSERT_EQ(scoreResult.value("status", std::string{}), "Succeeded")
        << scoreResult.dump(2);
    EXPECT_EQ(scoreResult["observedState"]["collectibleLifecycle"].value(
        "mode", std::string{}), "SingleCollectWin");
    EXPECT_EQ(scoreResult["observedState"]["collectibleLifecycle"].value(
        "score", -1), 20);

    const auto respawnResult = RunAuthoredArenaRush(
        respawnRoot, "blocks-respawn-runtime", false);
    ASSERT_FALSE(respawnResult.empty());
    ASSERT_EQ(respawnResult.value("status", std::string{}), "Succeeded")
        << respawnResult.dump(2);
    const auto& lifecycle =
        respawnResult["observedState"]["collectibleLifecycle"];
    EXPECT_GE(lifecycle.value("respawnTicks", -1), 29);
    EXPECT_LE(lifecycle.value("respawnTicks", -1), 31);
    EXPECT_EQ(lifecycle.value("pendingAfterCollect", -1), 5);
    EXPECT_EQ(lifecycle.value("pendingAfterRespawn", -1), 0);
    EXPECT_EQ(lifecycle.value("activeAfterRespawn", -1), 5);

    ASSERT_TRUE(WaitUntil([undo] { return undo->isEnabled(); }));
    undo->trigger();
    ASSERT_TRUE(WaitUntil([editor, respawn]
    {
        return editor->toPlainText().contains(
                   "EnergyCoreRespawnSeconds = 2.0") &&
            respawn->isEnabled() && respawn->value() == 2.0;
    }));
    ASSERT_TRUE(WaitUntil([undo] { return undo->isEnabled(); }));
    undo->trigger();
    ASSERT_TRUE(WaitUntil([editor, score]
    {
        return editor->toPlainText().contains("EnergyCoreScore = 10") &&
            score->isEnabled() && score->value() == 10;
    }));
    ASSERT_EQ(editor->toPlainText().toStdString(), checkedInSource);
    ASSERT_TRUE(save->isEnabled());
    save->click();
    ASSERT_TRUE(WaitUntil([&]
    { return ReadTextFile(scriptPath) == checkedInSource; }))
        << ReadTextFile(scriptPath);

    window.ReturnHome();
    ASSERT_TRUE(window.OpenProject(scoreRoot / "ArenaRush.sagaproj"));
    window.show();
    codeDock = window.findChild<QDockWidget*>("saga.panel.code_box");
    ASSERT_NE(codeDock, nullptr);
    codeDock->show();
    selector = codeDock->findChild<QComboBox*>();
    ASSERT_NE(selector, nullptr);
    selector->setCurrentIndex(selector->findText(
        QStringLiteral("ArenaRushGameMode.cs")));
    analyze = FindButton(codeDock, QStringLiteral("Analyze"));
    ASSERT_NE(analyze, nullptr);
    analyze->click();
    ASSERT_TRUE(WaitUntil([codeDock]
    {
        for (auto* candidate : codeDock->findChildren<QWidget*>())
            if (candidate->accessibleName() ==
                    QStringLiteral("Energy Core controlled fields") &&
                candidate->findChild<QSpinBox*>()->value() == 20 &&
                candidate->findChild<QDoubleSpinBox*>()->value() == 0.5 &&
                candidate->findChild<QSpinBox*>()->isEnabled())
                return true;
        return false;
    }));
    EXPECT_EQ(SagaEditor::HashDocumentBytes(ReadTextFile(
        scoreRoot / "Scripts/ArenaRushGameMode.cs")), authoredHash);
    window.ReturnHome();
    EXPECT_EQ(ReadTextFile(
        TestSourceRoot() / "Samples/ArenaRush/Scripts/ArenaRushGameMode.cs"),
        checkedInSource);
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
