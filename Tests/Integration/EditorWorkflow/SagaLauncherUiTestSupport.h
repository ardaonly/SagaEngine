// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "Launcher/SagaMainWindow.h"
#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Play/PlayPreparationPipeline.h"
#include "SagaEditor/Play/RuntimeProcessController.h"
#include "SagaEditor/Project/ProjectMutation.h"
#include "SagaEditor/Processes/ToolProcessRunner.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditorQt/QtUIMainWindow.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
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
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
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
constexpr const char* kWorkflowStateTimerObjectName = "SagaEditorWorkflowStateTimer";

class CloseOwningMainWindow final : public QMainWindow
{
public:
    int closeEventCount = 0;
protected:
    void closeEvent(QCloseEvent* event) override
    { ++closeEventCount; event->ignore(); }
};

std::filesystem::path TestSourceRoot()
{
    const auto configured = std::filesystem::path(SAGA_SOURCE_ROOT);
    if (std::filesystem::is_regular_file(configured / "Samples/StarterArena/StarterArena.sagaproj"))
        return configured;
    auto cursor = std::filesystem::current_path();
    for (;;)
    {
        if (std::filesystem::is_regular_file(cursor / "Samples/StarterArena/StarterArena.sagaproj"))
            return cursor;
        const auto parent = cursor.parent_path();
        if (parent == cursor) return configured;
        cursor = parent;
    }
}

bool WaitUntil(const std::function<bool()>& predicate, int timeoutMs = 30000)
{
    QElapsedTimer timer; timer.start();
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
    (void)excluded; return 0;
#else
    const auto parent = static_cast<std::uint64_t>(::getpid());
    for (const auto& entry : std::filesystem::directory_iterator("/proc"))
    {
        const auto name = entry.path().filename().string();
        if (name.empty() || !std::all_of(name.begin(), name.end(), ::isdigit)) continue;
        const auto pid = std::stoull(name);
        if (pid == excluded) continue;
        std::ifstream status(entry.path() / "status");
        std::string line; std::uint64_t processParent = 0;
        while (std::getline(status, line))
            if (line.rfind("PPid:", 0) == 0) { processParent = std::stoull(line.substr(5)); break; }
        if (processParent != parent) continue;
        std::ifstream commandInput(entry.path() / "cmdline", std::ios::binary);
        std::string command{std::istreambuf_iterator<char>(commandInput), {}};
        std::replace(command.begin(), command.end(), '\0', ' ');
        if (command.find("sagascript") != std::string::npos &&
            command.find("language-service") != std::string::npos) return pid;
    }
    return 0;
#endif
}

std::filesystem::path RuntimeExecutable()
{ return std::filesystem::path(SAGA_BUILD_ROOT) / "bin/SagaRuntime"; }

nlohmann::json RunAuthoredArenaRush(const std::filesystem::path& root,
                                    std::string session, bool singleCollect)
{
    const auto scenePath = root / "Scenes/ArenaRush.sagascene";
    std::ifstream input(scenePath, std::ios::binary);
    auto scene = nlohmann::json::parse(input); bool configured = false;
    for (auto& entity : scene["entities"])
        for (auto& component : entity["components"])
            if (component.value("type", std::string{}) == "ScriptBehavior" &&
                component.value("behaviorTypeId", std::string{}) == "ArenaRush.ArenaRushGameMode")
            {
                component["instanceConfiguration"]["targetProgress"] = singleCollect ? 20 : 200;
                configured = true;
            }
    EXPECT_TRUE(configured);
    std::ofstream(scenePath, std::ios::binary | std::ios::trunc) << scene.dump(2) << '\n';
    SagaEditor::EditorDocumentWorkspace workspace;
    EXPECT_TRUE(workspace.OpenProject(root / "ArenaRush.sagaproj"));
    const auto prepared = SagaEditor::PlayPreparationPipeline{}.Prepare(
        workspace, {std::move(session), RuntimeExecutable(), {}, {}, {}});
    EXPECT_TRUE(prepared.passed) << (prepared.diagnostics.empty() ? std::string{} :
        prepared.diagnostics.front().code + ": " + prepared.diagnostics.front().message);
    if (!prepared.passed) return {};
    SagaEditor::ToolProcessRequest request;
    request.executable = RuntimeExecutable();
    request.arguments = {"--play-envelope", prepared.localEnvelopePath.string(),
                         "--headless", "--smoke-interactions"};
    request.workingDirectory = prepared.buildRoot; request.inheritEnvironment = false;
    {
        std::ifstream envelopeInput(prepared.localEnvelopePath, std::ios::binary);
        const auto envelope = nlohmann::json::parse(envelopeInput);
        if (envelope.contains("environment") && envelope["environment"].is_object())
            for (const auto& [name, value] : envelope["environment"].items())
                if (value.is_string()) request.environment[name] = value.get<std::string>();
        for (const char* name : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                                 "XDG_RUNTIME_DIR", "DOTNET_ROOT", "LD_LIBRARY_PATH"})
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
        << "\nstdout=" << process.standardOutput << "\nstderr=" << process.standardError;
    if (!process.Succeeded() || !std::filesystem::is_regular_file(prepared.resultPath)) return {};
    std::ifstream result(prepared.resultPath, std::ios::binary);
    return nlohmann::json::parse(result);
}

void SetCursorAfter(QPlainTextEdit* editor, const QString& text, int occurrence = 0)
{
    auto cursor = editor->document()->find(text);
    while (occurrence-- > 0 && !cursor.isNull()) cursor = editor->document()->find(text, cursor);
    ASSERT_FALSE(cursor.isNull()); cursor.clearSelection();
    editor->setTextCursor(cursor); editor->setFocus();
}

void SendKey(QWidget* target, int key, Qt::KeyboardModifiers modifiers = {})
{
    QKeyEvent press(QEvent::KeyPress, key, modifiers); QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers); QApplication::sendEvent(target, &release);
    QApplication::processEvents();
}
} // namespace
