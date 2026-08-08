// SPDX-License-Identifier: MPL-2.0
/// @file SagaWorkModesUiTests.cpp
/// @brief Rendered product workflow for official modes and named workspaces.

#include "Launcher/SagaMainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextCursor>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
std::string ReadWorkModeFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

std::filesystem::path WorkModeSourceRoot()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT);
}

TEST(SagaWorkModesUiTest,
     XcbProgrammaticWidgetsPreserveArenaRushAcrossNamedWorkspaceWorkflow)
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
    std::filesystem::copy(WorkModeSourceRoot() / "Samples/ArenaRush",
        projectRoot, std::filesystem::copy_options::recursive);
    const auto projectPath = projectRoot / "ArenaRush.sagaproj";
    const auto lockPath = projectRoot / "SagaPackage.lock";
    const auto sourcePath = projectRoot / "Scripts/ArenaRushGameMode.cs";
    const std::string projectBefore = ReadWorkModeFile(projectPath);
    const std::string lockBefore = ReadWorkModeFile(lockPath);
    const std::string sourceBefore = ReadWorkModeFile(sourcePath);

    SagaProduct::SagaMainWindow window(
        QApplication::applicationFilePath().toStdString(), {},
        "quick-production");
    ASSERT_TRUE(window.OpenProject(projectPath));
    window.show();
    QApplication::processEvents();

    auto* workspaceDock =
        window.findChild<QDockWidget*>("saga.panel.customize_workspace");
    ASSERT_NE(workspaceDock, nullptr);
    workspaceDock->show();
    workspaceDock->raise();
    auto* mode = workspaceDock->findChild<QComboBox*>(
        "SagaWorkspaceWorkMode");
    auto* workspaces = workspaceDock->findChild<QComboBox*>(
        "SagaWorkspaceSelection");
    auto* name = workspaceDock->findChild<QLineEdit*>("SagaWorkspaceName");
    auto* saveAs = workspaceDock->findChild<QPushButton*>(
        "SagaWorkspaceSaveAs");
    auto* rename = workspaceDock->findChild<QPushButton*>(
        "SagaWorkspaceRename");
    auto* remove = workspaceDock->findChild<QPushButton*>(
        "SagaWorkspaceDelete");
    auto* appearance = workspaceDock->findChild<QComboBox*>(
        "SagaWorkspaceAppearance");
    ASSERT_NE(mode, nullptr);
    ASSERT_NE(workspaces, nullptr);
    ASSERT_NE(name, nullptr);
    ASSERT_NE(saveAs, nullptr);
    ASSERT_NE(rename, nullptr);
    ASSERT_NE(remove, nullptr);
    ASSERT_NE(appearance, nullptr);
    EXPECT_EQ(mode->currentData().toString(), QStringLiteral("quick-production"));

    auto* codeDock = window.findChild<QDockWidget*>("saga.panel.code_box");
    ASSERT_NE(codeDock, nullptr);
    codeDock->show();
    auto* sourceSelector = codeDock->findChild<QComboBox*>();
    ASSERT_NE(sourceSelector, nullptr);
    const int sourceIndex = sourceSelector->findText(
        QStringLiteral("ArenaRushGameMode.cs"));
    ASSERT_GE(sourceIndex, 0);
    sourceSelector->setCurrentIndex(sourceIndex);
    auto* editor = codeDock->findChild<QPlainTextEdit*>();
    ASSERT_NE(editor, nullptr);
    const QString unsavedMarker = QStringLiteral("\n// unsaved-work-mode-proof");
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText(unsavedMarker);
    const int cursorBefore = editor->textCursor().position();

    mode->setCurrentIndex(mode->findData(QStringLiteral("balanced")));
    QApplication::processEvents();
    EXPECT_EQ(mode->currentData().toString(), QStringLiteral("balanced"));
    EXPECT_TRUE(editor->toPlainText().endsWith(unsavedMarker));
    EXPECT_EQ(editor->textCursor().position(), cursorBefore);
    mode->setCurrentIndex(mode->findData(QStringLiteral("full-control")));
    QApplication::processEvents();
    EXPECT_EQ(mode->currentData().toString(), QStringLiteral("full-control"));
    EXPECT_TRUE(editor->toPlainText().endsWith(unsavedMarker));
    EXPECT_EQ(editor->textCursor().position(), cursorBefore);

    auto* performanceDock =
        window.findChild<QDockWidget*>("saga.panel.performance_transition");
    ASSERT_NE(performanceDock, nullptr);
    performanceDock->show();
    performanceDock->raise();
    auto* validateProfiles = performanceDock->findChild<QPushButton*>(
        "ValidateBuildProfilesButton");
    auto* createRunInput = performanceDock->findChild<QPushButton*>(
        "CreateBenchmarkRunInputButton");
    auto* reviewConversion = performanceDock->findChild<QPushButton*>(
        "ReviewCollectibleConversionButton");
    auto* compareConversion = performanceDock->findChild<QPushButton*>(
        "CompareCollectibleConversionButton");
    auto* reverseConversion = performanceDock->findChild<QPushButton*>(
        "ReverseCollectibleConversionButton");
    auto* benchmarkProfile = performanceDock->findChild<QComboBox*>(
        "BenchmarkProfileSelection");
    auto* performanceStatus = performanceDock->findChild<QLabel*>(
        "PerformanceTransitionStatus");
    auto* performanceOutput = performanceDock->findChild<QPlainTextEdit*>(
        "PerformanceRawResults");
    ASSERT_NE(validateProfiles, nullptr);
    ASSERT_NE(createRunInput, nullptr);
    ASSERT_NE(reviewConversion, nullptr);
    ASSERT_NE(compareConversion, nullptr);
    ASSERT_NE(reverseConversion, nullptr);
    ASSERT_NE(benchmarkProfile, nullptr);
    ASSERT_NE(performanceStatus, nullptr);
    ASSERT_NE(performanceOutput, nullptr);
    validateProfiles->click();
    QApplication::processEvents();
    EXPECT_EQ(benchmarkProfile->currentText(),
        QStringLiteral("arena-rush-collectibles"));
    createRunInput->click();
    QApplication::processEvents();
    EXPECT_TRUE(performanceOutput->toPlainText().contains(
        QStringLiteral("arena-rush.collectibles")));
    reviewConversion->click();
    QApplication::processEvents();
    EXPECT_TRUE(performanceOutput->toPlainText().contains(
        QStringLiteral("HighLevel")));
    compareConversion->click();
    QApplication::processEvents();
    EXPECT_TRUE(performanceOutput->toPlainText().contains(
        QStringLiteral("CollectiblesLowLevelBatch")));
    reverseConversion->click();
    QApplication::processEvents();
    EXPECT_EQ(performanceStatus->text(),
        QStringLiteral("No applied conversion is available to reverse."));
    ASSERT_TRUE(performanceDock->grab().save(QString::fromStdString(
        (evidence / "performance-transition-full-control.png").string())));

    workspaceDock->show();
    name->setText(QStringLiteral("Arena Authoring"));
    saveAs->click();
    QApplication::processEvents();
    ASSERT_GE(workspaces->findData(QStringLiteral("arena-authoring")), 0);
    EXPECT_EQ(workspaces->currentData().toString(),
        QStringLiteral("arena-authoring"));
    const QString identityBeforeAppearance = workspaces->currentData().toString();
    appearance->setCurrentText(QStringLiteral("Nord"));
    QApplication::processEvents();
    EXPECT_EQ(workspaces->currentData().toString(), identityBeforeAppearance);

    name->setText(QStringLiteral("Arena Review"));
    rename->click();
    QApplication::processEvents();
    EXPECT_GE(workspaces->findText(QStringLiteral("Arena Review")), 0);
    ASSERT_TRUE(workspaceDock->grab().save(QString::fromStdString(
        (evidence / "work-modes-named-workspace.png").string())));
    remove->click();
    QApplication::processEvents();
    EXPECT_EQ(workspaces->currentData().toString(), QString());
    EXPECT_EQ(workspaces->currentText(), QStringLiteral("Full Control Defaults"));

    EXPECT_EQ(ReadWorkModeFile(projectPath), projectBefore);
    EXPECT_EQ(ReadWorkModeFile(lockPath), lockBefore);
    EXPECT_EQ(ReadWorkModeFile(sourcePath), sourceBefore);
    EXPECT_TRUE(editor->toPlainText().endsWith(unsavedMarker));
    ASSERT_TRUE(window.grab().save(QString::fromStdString(
        (evidence / "work-modes-full-control.png").string())));
}
} // namespace
