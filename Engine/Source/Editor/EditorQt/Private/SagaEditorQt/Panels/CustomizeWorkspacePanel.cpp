/// @file CustomizeWorkspacePanel.cpp
/// @brief Qt backend implementation for the Safe Customize Workspace panel.

#include "SagaEditor/Panels/CustomizeWorkspacePanel.h"

#include "SagaEditor/Customization/WorkspaceCustomizationFeedback.h"
#include "SagaEditor/Customization/WorkspaceCustomizationPanelViewModel.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <string>
#include <utility>
#include <vector>

namespace SagaEditor
{
namespace
{

[[nodiscard]] QString BoolText(bool value)
{
    return value ? QStringLiteral("Visible") : QStringLiteral("Hidden");
}

[[nodiscard]] QString DirtyText(const WorkspaceCustomizationPanelRow& row)
{
    return row.pending ? QStringLiteral("Pending") : QStringLiteral("-");
}

[[nodiscard]] QString DiagnosticSeverityText(
    EditorCustomizationDiagnosticSeverity severity)
{
    switch (severity)
    {
    case EditorCustomizationDiagnosticSeverity::Info:
        return QStringLiteral("Info");
    case EditorCustomizationDiagnosticSeverity::Warning:
        return QStringLiteral("Warning");
    case EditorCustomizationDiagnosticSeverity::Error:
        return QStringLiteral("Error");
    case EditorCustomizationDiagnosticSeverity::Blocker:
        return QStringLiteral("Blocker");
    }
    return QStringLiteral("Error");
}

} // namespace

struct CustomizeWorkspacePanel::Impl
{
    explicit Impl(EditorHost& hostRef)
        : host(hostRef)
        , viewModel(hostRef.GetWorkspaceCustomizationSession())
    {
        widget = new QWidget();
        auto* root = new QVBoxLayout(widget);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        statusLabel = new QLabel(widget);
        statusLabel->setWordWrap(true);
        root->addWidget(statusLabel);

        tree = new QTreeWidget(widget);
        tree->setColumnCount(6);
        tree->setHeaderLabels({
            QStringLiteral("Visible"),
            QStringLiteral("Panel"),
            QStringLiteral("Current"),
            QStringLiteral("Default"),
            QStringLiteral("State"),
            QStringLiteral("Locked Reason"),
        });
        tree->setRootIsDecorated(false);
        tree->setUniformRowHeights(true);
        tree->setAlternatingRowColors(true);
        tree->header()->setStretchLastSection(true);
        tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        root->addWidget(tree);

        diagnosticsLabel = new QLabel(widget);
        diagnosticsLabel->setWordWrap(true);
        root->addWidget(diagnosticsLabel);

        auto* buttons = new QHBoxLayout();
        importButton = new QPushButton(QStringLiteral("Import..."), widget);
        exportButton = new QPushButton(QStringLiteral("Export..."), widget);
        saveButton = new QPushButton(QStringLiteral("Save"), widget);
        resetButton = new QPushButton(QStringLiteral("Reset"), widget);
        reloadButton = new QPushButton(QStringLiteral("Reload"), widget);
        buttons->addWidget(importButton);
        buttons->addWidget(exportButton);
        buttons->addStretch();
        buttons->addWidget(reloadButton);
        buttons->addWidget(resetButton);
        buttons->addWidget(saveButton);
        root->addLayout(buttons);

        QObject::connect(saveButton, &QPushButton::clicked,
            [this]()
            {
                const WorkspaceCustomizationSessionResult result =
                    viewModel.Save();
                Refresh();
                PublishResult(WorkspaceCustomizationFeedbackOperation::Save,
                              result,
                              true);
            });
        QObject::connect(resetButton, &QPushButton::clicked,
            [this]()
            {
                if (QMessageBox::question(
                        widget,
                        QStringLiteral("Reset Workspace Customization"),
                        QStringLiteral("Reset the saved workspace customization overlay?")) !=
                    QMessageBox::Yes)
                {
                    return;
                }
                const WorkspaceCustomizationSessionResult result =
                    viewModel.ResetWorkspace();
                Refresh();
                PublishResult(WorkspaceCustomizationFeedbackOperation::Reset,
                              result,
                              true);
            });
        QObject::connect(reloadButton, &QPushButton::clicked,
            [this]()
            {
                const WorkspaceCustomizationSessionResult result =
                    viewModel.Reload();
                Refresh();
                PublishResult(WorkspaceCustomizationFeedbackOperation::Reload,
                              result,
                              viewModel.GetState().restartRequired);
            });
        QObject::connect(importButton, &QPushButton::clicked,
            [this]()
            {
                const QString path = QFileDialog::getOpenFileName(
                    widget,
                    QStringLiteral("Import Workspace Customization"),
                    QString(),
                    QStringLiteral("Workspace overlays (*.json);;All files (*)"));
                if (path.isEmpty())
                {
                    return;
                }

                const WorkspaceCustomizationSessionResult result =
                    viewModel.ImportOverlay(path.toStdString());
                Refresh();
                PublishResult(WorkspaceCustomizationFeedbackOperation::Import,
                              result,
                              true);
            });
        QObject::connect(exportButton, &QPushButton::clicked,
            [this]()
            {
                const QString path = QFileDialog::getSaveFileName(
                    widget,
                    QStringLiteral("Export Workspace Customization"),
                    QStringLiteral("workspace.overlay.json"),
                    QStringLiteral("Workspace overlays (*.json);;All files (*)"));
                if (path.isEmpty())
                {
                    return;
                }

                const WorkspaceCustomizationSessionResult result =
                    viewModel.ExportOverlay(path.toStdString());
                Refresh();
                PublishResult(WorkspaceCustomizationFeedbackOperation::Export,
                              result,
                              false);
            });
    }

    void Refresh()
    {
        const WorkspaceCustomizationPanelViewState state =
            viewModel.GetState();
        RebuildRows(state.panels);
        UpdateStatus(state);
        UpdateDiagnostics(state.diagnostics);
    }

    void RebuildRows(const std::vector<WorkspaceCustomizationPanelRow>& rows)
    {
        tree->clear();
        for (const WorkspaceCustomizationPanelRow& row : rows)
        {
            auto* item = new QTreeWidgetItem(tree);
            item->setText(1, QStringLiteral("%1\n%2")
                              .arg(QString::fromStdString(row.displayName),
                                   QString::fromStdString(row.id)));
            item->setText(2, BoolText(row.currentVisible));
            item->setText(3, BoolText(row.defaultVisible));
            item->setText(4, DirtyText(row));
            item->setText(5, QString::fromStdString(row.lockedReason));
            item->setToolTip(1, QString::fromStdString(row.id));

            auto* toggle = new QCheckBox(tree);
            toggle->setChecked(row.visible);
            toggle->setEnabled(row.editable);
            toggle->setToolTip(row.editable
                                   ? QStringLiteral("Change safe panel visibility")
                                   : QString::fromStdString(row.lockedReason));
            tree->setItemWidget(item, 0, toggle);

            QObject::connect(toggle, &QCheckBox::toggled,
                [this, panelId = row.id](bool visible)
                {
                    (void)viewModel.TogglePanel(panelId, visible);
                    Refresh();
                });
        }
    }

    void UpdateStatus(const WorkspaceCustomizationPanelViewState& state)
    {
        QString text = QStringLiteral("Status: %1")
            .arg(QString::fromStdString(state.statusText));
        if (!state.overlayPath.empty())
        {
            text += QStringLiteral("\nOverlay: %1")
                .arg(QString::fromStdString(state.overlayPath));
        }
        if (state.restartRequired)
        {
            text += QStringLiteral(
                "\nRestart required: workspace customization changes are saved as an overlay and apply on next editor startup.");
        }
        else if (state.dirty)
        {
            text += QStringLiteral("\nWorkspace customization has pending changes.");
        }
        statusLabel->setText(text);
        saveButton->setEnabled(state.canSave);
        resetButton->setEnabled(state.canReset);
        reloadButton->setEnabled(state.canReset);
        importButton->setEnabled(state.canReset);
        exportButton->setEnabled(state.status == WorkspaceCustomizationStatus::Ready);
    }

    void UpdateDiagnostics(
        const std::vector<WorkspaceCustomizationDiagnosticRow>& diagnostics)
    {
        if (diagnostics.empty())
        {
            diagnosticsLabel->setText(QStringLiteral("No customization diagnostics."));
            return;
        }

        QStringList lines;
        for (const WorkspaceCustomizationDiagnosticRow& diagnostic : diagnostics)
        {
            lines << QStringLiteral("%1 %2: %3")
                         .arg(DiagnosticSeverityText(diagnostic.severity),
                              QString::fromStdString(diagnostic.code),
                              QString::fromStdString(diagnostic.message));
        }
        diagnosticsLabel->setText(lines.join(QStringLiteral("\n")));
    }

    void PublishResult(WorkspaceCustomizationFeedbackOperation operation,
                       const WorkspaceCustomizationSessionResult& result,
                       bool restartRequired)
    {
        host.GetEditorNotificationCenter().Publish(
            MakeWorkspaceCustomizationNotification(operation,
                                                   result,
                                                   restartRequired));
    }

    EditorHost& host;
    WorkspaceCustomizationPanelViewModel viewModel;
    QWidget* widget = nullptr;
    QLabel* statusLabel = nullptr;
    QTreeWidget* tree = nullptr;
    QLabel* diagnosticsLabel = nullptr;
    QPushButton* importButton = nullptr;
    QPushButton* exportButton = nullptr;
    QPushButton* saveButton = nullptr;
    QPushButton* resetButton = nullptr;
    QPushButton* reloadButton = nullptr;
};

CustomizeWorkspacePanel::CustomizeWorkspacePanel(EditorHost& host)
    : m_impl(std::make_unique<Impl>(host))
{}

CustomizeWorkspacePanel::~CustomizeWorkspacePanel() = default;

PanelId CustomizeWorkspacePanel::GetPanelId() const
{
    return "saga.panel.customize_workspace";
}

std::string CustomizeWorkspacePanel::GetTitle() const
{
    return "Customize Workspace";
}

void* CustomizeWorkspacePanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void CustomizeWorkspacePanel::OnInit()
{
    Refresh();
}

void CustomizeWorkspacePanel::OnShutdown() {}

void CustomizeWorkspacePanel::Refresh()
{
    m_impl->Refresh();
}

} // namespace SagaEditor
