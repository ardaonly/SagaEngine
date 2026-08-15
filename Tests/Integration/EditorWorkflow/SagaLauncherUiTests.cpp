// SPDX-License-Identifier: MPL-2.0
/// @file SagaLauncherUiTests.cpp
/// @brief Rendered package workflow and recovery contracts.

#include "SagaLauncherUiTestSupport.h"

namespace
{
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
    ASSERT_TRUE(MaterializeArenaRushProject(projectRoot));
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
        ASSERT_TRUE(WaitUntil([analyze] { return analyze->isEnabled(); }, 60000));
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
