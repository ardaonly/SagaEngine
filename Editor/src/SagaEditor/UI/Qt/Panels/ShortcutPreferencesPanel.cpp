/// @file ShortcutPreferencesPanel.cpp
/// @brief Qt backend implementation for the Safe Shortcut Preferences panel.

#include "SagaEditor/Panels/ShortcutPreferencesPanel.h"

#include "SagaEditor/Customization/ShortcutCustomizationFeedback.h"
#include "SagaEditor/Customization/ShortcutCustomizationPanelViewModel.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"

#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <string>
#include <vector>

namespace SagaEditor
{
namespace
{

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

[[nodiscard]] QString PendingText(const ShortcutCustomizationActionRow& row)
{
    return row.pending ? QStringLiteral("Pending") : QStringLiteral("-");
}

} // namespace

struct ShortcutPreferencesPanel::Impl
{
    explicit Impl(EditorHost& hostRef)
        : host(hostRef)
        , viewModel(hostRef.GetShortcutCustomizationSession())
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
            QStringLiteral("Action"),
            QStringLiteral("Current"),
            QStringLiteral("Effective"),
            QStringLiteral("State"),
            QStringLiteral("Locked Reason"),
            QStringLiteral("Id"),
        });
        tree->setRootIsDecorated(false);
        tree->setUniformRowHeights(true);
        tree->setAlternatingRowColors(true);
        tree->header()->setStretchLastSection(false);
        tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        root->addWidget(tree);

        auto* editor = new QHBoxLayout();
        chordEdit = new QLineEdit(widget);
        chordEdit->setPlaceholderText(QStringLiteral("Ctrl+S"));
        setButton = new QPushButton(QStringLiteral("Set"), widget);
        clearButton = new QPushButton(QStringLiteral("Clear"), widget);
        resetActionButton = new QPushButton(QStringLiteral("Reset Action"), widget);
        editor->addWidget(chordEdit);
        editor->addWidget(setButton);
        editor->addWidget(clearButton);
        editor->addWidget(resetActionButton);
        root->addLayout(editor);

        diagnosticsLabel = new QLabel(widget);
        diagnosticsLabel->setWordWrap(true);
        root->addWidget(diagnosticsLabel);

        auto* buttons = new QHBoxLayout();
        importButton = new QPushButton(QStringLiteral("Import..."), widget);
        exportButton = new QPushButton(QStringLiteral("Export..."), widget);
        saveButton = new QPushButton(QStringLiteral("Save"), widget);
        resetAllButton = new QPushButton(QStringLiteral("Reset All"), widget);
        reloadButton = new QPushButton(QStringLiteral("Reload"), widget);
        buttons->addWidget(importButton);
        buttons->addWidget(exportButton);
        buttons->addStretch();
        buttons->addWidget(reloadButton);
        buttons->addWidget(resetAllButton);
        buttons->addWidget(saveButton);
        root->addLayout(buttons);

        QObject::connect(tree, &QTreeWidget::itemSelectionChanged,
            [this]() { UpdateSelectionState(); });
        QObject::connect(setButton, &QPushButton::clicked,
            [this]()
            {
                const std::string actionId = SelectedActionId();
                if (!actionId.empty())
                {
                    (void)viewModel.SetShortcut(actionId,
                                                chordEdit->text().toStdString());
                    Refresh();
                }
            });
        QObject::connect(clearButton, &QPushButton::clicked,
            [this]()
            {
                const std::string actionId = SelectedActionId();
                if (!actionId.empty())
                {
                    (void)viewModel.ClearShortcut(actionId);
                    Refresh();
                }
            });
        QObject::connect(resetActionButton, &QPushButton::clicked,
            [this]()
            {
                const std::string actionId = SelectedActionId();
                if (!actionId.empty())
                {
                    (void)viewModel.ResetShortcut(actionId);
                    Refresh();
                }
            });
        QObject::connect(saveButton, &QPushButton::clicked,
            [this]()
            {
                const ShortcutCustomizationSessionResult result =
                    viewModel.Save();
                Refresh();
                PublishResult(ShortcutCustomizationFeedbackOperation::Save,
                              result,
                              true);
            });
        QObject::connect(resetAllButton, &QPushButton::clicked,
            [this]()
            {
                if (QMessageBox::question(
                        widget,
                        QStringLiteral("Reset Shortcut Preferences"),
                        QStringLiteral("Reset saved shortcut preferences?")) !=
                    QMessageBox::Yes)
                {
                    return;
                }
                const ShortcutCustomizationSessionResult result =
                    viewModel.ResetAll();
                Refresh();
                PublishResult(ShortcutCustomizationFeedbackOperation::Reset,
                              result,
                              true);
            });
        QObject::connect(reloadButton, &QPushButton::clicked,
            [this]()
            {
                const ShortcutCustomizationSessionResult result =
                    viewModel.Reload();
                Refresh();
                PublishResult(ShortcutCustomizationFeedbackOperation::Reload,
                              result,
                              viewModel.GetState().restartRequired);
            });
        QObject::connect(importButton, &QPushButton::clicked,
            [this]()
            {
                const QString path = QFileDialog::getOpenFileName(
                    widget,
                    QStringLiteral("Import Shortcut Preferences"),
                    QString(),
                    QStringLiteral("Safe customization overlays (*.json);;All files (*)"));
                if (path.isEmpty())
                {
                    return;
                }
                const ShortcutCustomizationSessionResult result =
                    viewModel.ImportOverlay(path.toStdString());
                Refresh();
                PublishResult(ShortcutCustomizationFeedbackOperation::Import,
                              result,
                              true);
            });
        QObject::connect(exportButton, &QPushButton::clicked,
            [this]()
            {
                const QString path = QFileDialog::getSaveFileName(
                    widget,
                    QStringLiteral("Export Shortcut Preferences"),
                    QStringLiteral("shortcut.preferences.overlay.json"),
                    QStringLiteral("Safe customization overlays (*.json);;All files (*)"));
                if (path.isEmpty())
                {
                    return;
                }
                const ShortcutCustomizationSessionResult result =
                    viewModel.ExportOverlay(path.toStdString());
                Refresh();
                PublishResult(ShortcutCustomizationFeedbackOperation::Export,
                              result,
                              false);
            });
    }

    void Refresh()
    {
        const ShortcutCustomizationPanelViewState state =
            viewModel.GetState();
        RebuildRows(state.actions);
        UpdateStatus(state);
        UpdateDiagnostics(state.diagnostics);
        UpdateSelectionState();
    }

    void RebuildRows(const std::vector<ShortcutCustomizationActionRow>& rows)
    {
        const std::string previousSelection = SelectedActionId();
        tree->clear();
        for (const ShortcutCustomizationActionRow& row : rows)
        {
            auto* item = new QTreeWidgetItem(tree);
            item->setText(0, QString::fromStdString(row.displayName));
            item->setText(1, QString::fromStdString(row.currentChord));
            item->setText(2, QString::fromStdString(row.effectiveChord));
            item->setText(3, PendingText(row));
            item->setText(4, QString::fromStdString(row.lockedReason));
            item->setText(5, QString::fromStdString(row.actionId));
            item->setData(0, Qt::UserRole,
                          QString::fromStdString(row.actionId));
            item->setData(0, Qt::UserRole + 1, row.editable);
            item->setToolTip(0, QString::fromStdString(row.actionId));
            if (row.actionId == previousSelection)
            {
                item->setSelected(true);
                tree->setCurrentItem(item);
            }
        }
    }

    void UpdateStatus(const ShortcutCustomizationPanelViewState& state)
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
                "\nRestart required: shortcut changes are saved as an overlay and apply on next editor startup.");
        }
        else if (state.dirty)
        {
            text += QStringLiteral("\nShortcut preferences have pending changes.");
        }
        statusLabel->setText(text);
        saveButton->setEnabled(state.canSave);
        resetAllButton->setEnabled(state.canReset);
        reloadButton->setEnabled(state.canReset);
        importButton->setEnabled(state.canReset);
        exportButton->setEnabled(state.status == ShortcutCustomizationStatus::Ready);
    }

    void UpdateDiagnostics(
        const std::vector<ShortcutCustomizationDiagnosticRow>& diagnostics)
    {
        if (diagnostics.empty())
        {
            diagnosticsLabel->setText(QStringLiteral("No shortcut diagnostics."));
            return;
        }

        QStringList lines;
        for (const ShortcutCustomizationDiagnosticRow& diagnostic : diagnostics)
        {
            lines << QStringLiteral("%1 %2: %3")
                         .arg(DiagnosticSeverityText(diagnostic.severity),
                              QString::fromStdString(diagnostic.code),
                              QString::fromStdString(diagnostic.message));
        }
        diagnosticsLabel->setText(lines.join(QStringLiteral("\n")));
    }

    void UpdateSelectionState()
    {
        QTreeWidgetItem* item = tree->currentItem();
        const bool editable =
            item != nullptr && item->data(0, Qt::UserRole + 1).toBool();
        chordEdit->setEnabled(editable);
        setButton->setEnabled(editable);
        clearButton->setEnabled(editable);
        resetActionButton->setEnabled(item != nullptr);
        if (item != nullptr)
        {
            chordEdit->setText(item->text(2));
        }
        else
        {
            chordEdit->clear();
        }
    }

    [[nodiscard]] std::string SelectedActionId() const
    {
        const QTreeWidgetItem* item = tree->currentItem();
        if (item == nullptr)
        {
            return {};
        }
        return item->data(0, Qt::UserRole).toString().toStdString();
    }

    void PublishResult(ShortcutCustomizationFeedbackOperation operation,
                       const ShortcutCustomizationSessionResult& result,
                       bool restartRequired)
    {
        host.GetEditorNotificationCenter().Publish(
            MakeShortcutCustomizationNotification(operation,
                                                  result,
                                                  restartRequired));
    }

    EditorHost& host;
    ShortcutCustomizationPanelViewModel viewModel;
    QWidget* widget = nullptr;
    QLabel* statusLabel = nullptr;
    QTreeWidget* tree = nullptr;
    QLineEdit* chordEdit = nullptr;
    QLabel* diagnosticsLabel = nullptr;
    QPushButton* importButton = nullptr;
    QPushButton* exportButton = nullptr;
    QPushButton* saveButton = nullptr;
    QPushButton* resetAllButton = nullptr;
    QPushButton* reloadButton = nullptr;
    QPushButton* setButton = nullptr;
    QPushButton* clearButton = nullptr;
    QPushButton* resetActionButton = nullptr;
};

ShortcutPreferencesPanel::ShortcutPreferencesPanel(EditorHost& host)
    : m_impl(std::make_unique<Impl>(host))
{}

ShortcutPreferencesPanel::~ShortcutPreferencesPanel() = default;

PanelId ShortcutPreferencesPanel::GetPanelId() const
{
    return "saga.panel.shortcut_preferences";
}

std::string ShortcutPreferencesPanel::GetTitle() const
{
    return "Shortcut Preferences";
}

void* ShortcutPreferencesPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void ShortcutPreferencesPanel::OnInit()
{
    Refresh();
}

void ShortcutPreferencesPanel::OnShutdown() {}

void ShortcutPreferencesPanel::Refresh()
{
    m_impl->Refresh();
}

} // namespace SagaEditor
