/// @file SagaLauncherUiTests.cpp
/// @brief Offscreen durable minimum Product Launcher UI contracts.

#include "SagaLauncherWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QGroupBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTreeWidget>
#include <algorithm>

namespace
{

TEST(SagaLauncherUiTest, DurableMinimumSectionsAndActionsExist)
{
    SagaProduct::SagaLauncherWindow window(QApplication::applicationFilePath().toStdString());
    QApplication::processEvents();

    for (const char* name : {"SagaLauncherProjectSection",
                             "SagaLauncherTargetsSection",
                             "SagaLauncherReportsSection",
                             "SagaLauncherDistributionSection",
                             "SagaLauncherLimitationsSection",
                             "SagaLauncherDiagnosticsSection"})
    {
        EXPECT_NE(window.findChild<QGroupBox*>(name), nullptr) << name;
    }
    EXPECT_NE(window.findChild<QPushButton*>("SagaLauncherOpenProjectButton"), nullptr);
    EXPECT_NE(window.findChild<QPushButton*>("SagaLauncherOpenEditorButton"), nullptr);
    EXPECT_NE(window.findChild<QPushButton*>("SagaLauncherValidateProjectButton"), nullptr);
    EXPECT_NE(window.findChild<QProgressBar*>("SagaLauncherActionProgress"), nullptr);
    EXPECT_NE(window.findChild<QPushButton*>("SagaLauncherCancelActionButton"), nullptr);
}

TEST(SagaLauncherUiTest, ForbiddenLegacyAndRawCommandControlsAreAbsent)
{
    SagaProduct::SagaLauncherWindow window(QApplication::applicationFilePath().toStdString());
    QApplication::processEvents();

    QStringList labels;
    for (const auto* button : window.findChildren<QPushButton*>())
        labels.push_back(button->text());
    const QString joined = labels.join('|');
    EXPECT_FALSE(joined.contains("Create Project"));
    EXPECT_FALSE(joined.contains("Host Session"));
    EXPECT_FALSE(joined.contains("Join Room"));
    EXPECT_FALSE(joined.contains("command", Qt::CaseInsensitive));
    EXPECT_FALSE(joined.contains("EditorLab"));
    EXPECT_FALSE(joined.contains("Sandbox"));
}

TEST(SagaLauncherUiTest, UnsupportedTargetRowsAreVisibleAndDisabled)
{
    SagaProduct::SagaLauncherWindow window(QApplication::applicationFilePath().toStdString());
    QApplication::processEvents();
    auto* targets = window.findChild<QTreeWidget*>("SagaLauncherTargets");
    ASSERT_NE(targets, nullptr);

    int unsupported = 0;
    for (int index = 0; index < targets->topLevelItemCount(); ++index)
    {
        const auto* item = targets->topLevelItem(index);
        if (item->text(1) == "unsupported")
        {
            ++unsupported;
            EXPECT_FALSE(item->flags().testFlag(Qt::ItemIsEnabled));
        }
    }
    EXPECT_EQ(unsupported, 4);
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
