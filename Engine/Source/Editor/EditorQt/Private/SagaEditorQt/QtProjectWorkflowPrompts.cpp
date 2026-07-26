// SPDX-License-Identifier: MPL-2.0

#include "SagaEditorQt/QtProjectWorkflowPrompts.h"

#include <QApplication>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QString>
#include <QWidget>

namespace SagaEditor
{

ProjectWorkflowPrompts MakeQtProjectWorkflowPrompts()
{
    ProjectWorkflowPrompts prompts;
    prompts.chooseExistingProject = []()
        -> std::optional<std::filesystem::path>
    {
        const auto path = QFileDialog::getOpenFileName(
            QApplication::activeWindow(),
            QStringLiteral("Open Saga Project"), {},
            QStringLiteral("Saga Projects (*.sagaproj)"));
        if (path.isEmpty()) return std::nullopt;
        return std::filesystem::path(path.toStdString());
    };
    prompts.requestNewProject = []()
        -> std::optional<ProjectCreationPrompt>
    {
        bool accepted = false;
        const auto name = QInputDialog::getText(
            QApplication::activeWindow(),
            QStringLiteral("New Starter Arena Project"),
            QStringLiteral("Project name"), QLineEdit::Normal,
            QStringLiteral("StarterArena"), &accepted).trimmed();
        if (!accepted || name.isEmpty()) return std::nullopt;
        const auto parent = QFileDialog::getExistingDirectory(
            QApplication::activeWindow(),
            QStringLiteral("Choose Project Parent Directory"));
        if (parent.isEmpty()) return std::nullopt;
        return ProjectCreationPrompt{
            name.toStdString(),
            std::filesystem::path(parent.toStdString()) / name.toStdString()};
    };
    prompts.confirmDirtyDocuments =
        [](const std::vector<std::string>& documentIds)
    {
        QString detail;
        for (const auto& id : documentIds)
            detail += QString::fromStdString(id) + QLatin1Char('\n');
        QMessageBox box(
            QMessageBox::Warning,
            QStringLiteral("Unsaved source documents"),
            QStringLiteral(
                "Save canonical source changes before continuing?"),
            QMessageBox::Save | QMessageBox::Discard |
                QMessageBox::Cancel,
            QApplication::activeWindow());
        box.setDetailedText(detail.trimmed());
        box.setDefaultButton(QMessageBox::Save);
        const auto decision = box.exec();
        if (decision == QMessageBox::Save)
            return DirtyDocumentDecision::SaveAll;
        if (decision == QMessageBox::Discard)
            return DirtyDocumentDecision::Discard;
        return DirtyDocumentDecision::Cancel;
    };
    return prompts;
}

} // namespace SagaEditor
