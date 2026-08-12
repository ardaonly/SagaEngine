// SPDX-License-Identifier: MPL-2.0
/// @file SagaLauncherControlledBlocksTests.cpp
/// @brief Controlled Blocks recovery and runtime semantics.

#include "SagaLauncherUiTestSupport.h"

namespace
{
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

