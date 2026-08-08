// SPDX-License-Identifier: MPL-2.0
/// @file CustomizeWorkspacePanel.cpp
/// @brief Qt backend implementation for the Safe Customize Workspace panel.

#include "SagaEditor/Panels/CustomizeWorkspacePanel.h"

#include "SagaEditor/Customization/WorkspaceCustomizationFeedback.h"
#include "SagaEditor/Customization/WorkspaceCustomizationPanelViewModel.h"
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"
#include "SagaEditor/Themes/ThemeRegistry.h"
#include "SagaEditor/Workspaces/UserWorkspace.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
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

        auto* workspaceRow = new QHBoxLayout();
        modeCombo = new QComboBox(widget);
        modeCombo->setObjectName(QStringLiteral("SagaWorkspaceWorkMode"));
        for (const auto& mode : host.GetEditorWorkModeService().Modes())
            modeCombo->addItem(QString::fromStdString(mode.displayName),
                QString::fromStdString(mode.id));
        workspaceCombo = new QComboBox(widget);
        workspaceCombo->setObjectName(QStringLiteral("SagaWorkspaceSelection"));
        workspaceName = new QLineEdit(widget);
        workspaceName->setObjectName(QStringLiteral("SagaWorkspaceName"));
        workspaceName->setPlaceholderText(QStringLiteral("Workspace name"));
        workspaceRow->addWidget(new QLabel(QStringLiteral("Mode:"), widget));
        workspaceRow->addWidget(modeCombo);
        workspaceRow->addWidget(new QLabel(QStringLiteral("Workspace:"), widget));
        workspaceRow->addWidget(workspaceCombo);
        workspaceRow->addWidget(workspaceName);
        root->addLayout(workspaceRow);

        auto* workspaceButtons = new QHBoxLayout();
        saveAsButton = new QPushButton(QStringLiteral("Save As"), widget);
        saveAsButton->setObjectName(QStringLiteral("SagaWorkspaceSaveAs"));
        loadWorkspaceButton = new QPushButton(QStringLiteral("Load"), widget);
        loadWorkspaceButton->setObjectName(QStringLiteral("SagaWorkspaceLoad"));
        renameWorkspaceButton = new QPushButton(QStringLiteral("Rename"), widget);
        renameWorkspaceButton->setObjectName(QStringLiteral("SagaWorkspaceRename"));
        deleteWorkspaceButton = new QPushButton(QStringLiteral("Delete"), widget);
        deleteWorkspaceButton->setObjectName(QStringLiteral("SagaWorkspaceDelete"));
        resetCurrentButton = new QPushButton(QStringLiteral("Reset Current Workspace"), widget);
        resetCurrentButton->setObjectName(QStringLiteral("SagaWorkspaceResetCurrent"));
        resetModeButton = new QPushButton(QStringLiteral("Reset Mode Defaults"), widget);
        resetModeButton->setObjectName(QStringLiteral("SagaWorkspaceResetMode"));
        appearanceCombo = new QComboBox(widget);
        appearanceCombo->setObjectName(QStringLiteral("SagaWorkspaceAppearance"));
        appearanceCombo->addItems({QStringLiteral("Dark"), QStringLiteral("Light"),
            QStringLiteral("Midnight"), QStringLiteral("Nord")});
        workspaceButtons->addWidget(saveAsButton);
        workspaceButtons->addWidget(loadWorkspaceButton);
        workspaceButtons->addWidget(renameWorkspaceButton);
        workspaceButtons->addWidget(deleteWorkspaceButton);
        workspaceButtons->addWidget(resetCurrentButton);
        workspaceButtons->addWidget(resetModeButton);
        workspaceButtons->addStretch();
        workspaceButtons->addWidget(new QLabel(QStringLiteral("Appearance:"), widget));
        workspaceButtons->addWidget(appearanceCombo);
        root->addLayout(workspaceButtons);

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
        saveButton = new QPushButton(QStringLiteral("Save Workspace"), widget);
        resetButton = new QPushButton(QStringLiteral("Reset Current"), widget);
        reloadButton = new QPushButton(QStringLiteral("Reload"), widget);
        importButton->hide();
        exportButton->hide();
        reloadButton->hide();
        buttons->addWidget(importButton);
        buttons->addWidget(exportButton);
        buttons->addStretch();
        buttons->addWidget(reloadButton);
        buttons->addWidget(resetButton);
        buttons->addWidget(saveButton);
        root->addLayout(buttons);

        QObject::connect(modeCombo, &QComboBox::currentIndexChanged,
            [this]
            {
                const auto id = modeCombo->currentData().toString().toStdString();
                (void)host.GetEditorWorkModeService().SetActive(id);
                RefreshPersonalWorkspaces();
            });
        QObject::connect(saveAsButton, &QPushButton::clicked,
            [this]
            {
                const auto name = workspaceName->text().trimmed().toStdString();
                if (name.empty()) return;
                (void)host.GetCommandDispatcher().Dispatch(
                    "saga.command.workspace.capture");
                if (host.GetUserWorkspaceService().SaveAs(name))
                    (void)host.GetCommandDispatcher().Dispatch(
                        "saga.command.workspace.apply");
                RefreshPersonalWorkspaces();
            });
        QObject::connect(loadWorkspaceButton, &QPushButton::clicked,
            [this]
            {
                const auto id = workspaceCombo->currentData().toString().toStdString();
                if (id.empty() || !host.GetUserWorkspaceService().Load(id)) return;
                (void)host.GetCommandDispatcher().Dispatch("saga.command.workspace.apply");
                RefreshPersonalWorkspaces();
            });
        QObject::connect(renameWorkspaceButton, &QPushButton::clicked,
            [this]
            {
                const auto id = workspaceCombo->currentData().toString().toStdString();
                const auto name = workspaceName->text().trimmed().toStdString();
                if (!id.empty() && !name.empty())
                    (void)host.GetUserWorkspaceService().Rename(id, name);
                RefreshPersonalWorkspaces();
            });
        QObject::connect(deleteWorkspaceButton, &QPushButton::clicked,
            [this]
            {
                const auto id = workspaceCombo->currentData().toString().toStdString();
                if (!id.empty() && host.GetUserWorkspaceService().Delete(id))
                    (void)host.GetCommandDispatcher().Dispatch(
                        "saga.command.workspace.apply");
                RefreshPersonalWorkspaces();
            });
        QObject::connect(resetCurrentButton, &QPushButton::clicked,
            [this]
            {
                (void)host.GetUserWorkspaceService().ResetCurrentWorkspace();
                (void)host.GetCommandDispatcher().Dispatch("saga.command.workspace.apply");
                RefreshPersonalWorkspaces();
            });
        QObject::connect(resetModeButton, &QPushButton::clicked,
            [this]
            {
                (void)host.GetUserWorkspaceService().ResetModeDefaults();
                (void)host.GetCommandDispatcher().Dispatch("saga.command.workspace.apply");
                RefreshPersonalWorkspaces();
            });
        QObject::connect(appearanceCombo, &QComboBox::currentTextChanged,
            [this](const QString& value)
            {
                auto appearance = host.GetUserWorkspaceService().Appearance();
                appearance.theme = value.toStdString();
                if (host.GetUserWorkspaceService().SetAppearance(appearance))
                    (void)host.GetThemeRegistry().Apply(appearance.theme);
            });

        QObject::connect(saveButton, &QPushButton::clicked,
            [this]()
            {
                (void)host.GetUserWorkspaceService().SaveCurrent();
                (void)host.GetCommandDispatcher().Dispatch(
                    "saga.command.workspace.apply");
                Refresh();
            });
        QObject::connect(resetButton, &QPushButton::clicked,
            [this]()
            {
                (void)host.GetUserWorkspaceService().ResetCurrentWorkspace();
                (void)host.GetCommandDispatcher().Dispatch(
                    "saga.command.workspace.apply");
                Refresh();
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
        RefreshPersonalWorkspaces();
        const WorkspaceCustomizationPanelViewState state =
            viewModel.GetState();
        RebuildRows(state.panels);
        UpdateStatus(state);
        UpdateDiagnostics(state.diagnostics);
    }

    void RefreshPersonalWorkspaces()
    {
        const QSignalBlocker modeBlocker(modeCombo);
        const QSignalBlocker workspaceBlocker(workspaceCombo);
        const QSignalBlocker appearanceBlocker(appearanceCombo);
        const auto& modes = host.GetEditorWorkModeService();
        modeCombo->setCurrentIndex(modeCombo->findData(
            QString::fromStdString(modes.Active().id)));
        workspaceCombo->clear();
        workspaceCombo->addItem(QString::fromStdString(
            modes.Active().displayName + " Defaults"), QString());
        const auto& state = host.GetUserWorkspaceService().State();
        for (const auto& workspace : state.namedWorkspaces)
            if (workspace.modeId == modes.Active().id)
                workspaceCombo->addItem(QString::fromStdString(workspace.displayName),
                    QString::fromStdString(workspace.id));
        const int selected = workspaceCombo->findData(
            QString::fromStdString(state.currentWorkspaceId));
        workspaceCombo->setCurrentIndex(selected < 0 ? 0 : selected);
        appearanceCombo->setCurrentText(QString::fromStdString(
            host.GetUserWorkspaceService().Appearance().theme));
        const bool named = !workspaceCombo->currentData().toString().isEmpty();
        loadWorkspaceButton->setEnabled(named);
        renameWorkspaceButton->setEnabled(named);
        deleteWorkspaceButton->setEnabled(named);
    }

    void RebuildRows(const std::vector<WorkspaceCustomizationPanelRow>& rows)
    {
        tree->clear();
        const auto effective = host.GetUserWorkspaceService().Effective();
        for (const WorkspaceCustomizationPanelRow& row : rows)
        {
            const auto effectiveDock = std::find_if(effective.docks.begin(),
                effective.docks.end(), [&row](const WorkspaceDockDefinition& dock)
                { return dock.panelId == row.id; });
            const bool effectiveVisible = effectiveDock != effective.docks.end() &&
                effectiveDock->visible;
            auto* item = new QTreeWidgetItem(tree);
            item->setText(1, QStringLiteral("%1\n%2")
                              .arg(QString::fromStdString(row.displayName),
                                   QString::fromStdString(row.id)));
            item->setText(2, BoolText(effectiveVisible));
            item->setText(3, BoolText(row.defaultVisible));
            item->setText(4, DirtyText(row));
            item->setText(5, QString::fromStdString(row.lockedReason));
            item->setToolTip(1, QString::fromStdString(row.id));

            auto* toggle = new QCheckBox(tree);
            toggle->setChecked(effectiveVisible);
            toggle->setEnabled(row.editable);
            toggle->setToolTip(row.editable
                                   ? QStringLiteral("Change safe panel visibility")
                                   : QString::fromStdString(row.lockedReason));
            tree->setItemWidget(item, 0, toggle);

            QObject::connect(toggle, &QCheckBox::toggled,
                [this, panelId = row.id](bool visible)
                {
                    (void)host.GetUserWorkspaceService().SetPanelVisible(
                        panelId, visible);
                    (void)host.GetCommandDispatcher().Dispatch(
                        "saga.command.workspace.apply");
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
        const bool dirty = host.GetUserWorkspaceService().State().dirty;
        text += dirty ? QStringLiteral("\nCurrent workspace has unsaved changes.")
                      : QStringLiteral("\nCurrent workspace is saved.");
        statusLabel->setText(text);
        saveButton->setEnabled(dirty);
        resetButton->setEnabled(true);
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
    QComboBox* modeCombo = nullptr;
    QComboBox* workspaceCombo = nullptr;
    QComboBox* appearanceCombo = nullptr;
    QLineEdit* workspaceName = nullptr;
    QPushButton* saveAsButton = nullptr;
    QPushButton* loadWorkspaceButton = nullptr;
    QPushButton* renameWorkspaceButton = nullptr;
    QPushButton* deleteWorkspaceButton = nullptr;
    QPushButton* resetCurrentButton = nullptr;
    QPushButton* resetModeButton = nullptr;
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

QWidget* CustomizeWorkspacePanel::Widget() const noexcept
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
