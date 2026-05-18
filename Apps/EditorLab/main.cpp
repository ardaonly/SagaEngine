/// @file main.cpp
/// @brief Standalone EditorLab entry point for scenario runner experiments.

#include "SagaEditorLab/ScenarioRunnerPanel.h"

#include <QApplication>
#include <QWidget>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    SagaEditorLab::ScenarioRunnerPanel panel;
    panel.Widget()->show();

    return app.exec();
}
