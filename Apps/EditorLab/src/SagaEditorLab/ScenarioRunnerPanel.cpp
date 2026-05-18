/// @file ScenarioRunnerPanel.cpp
/// @brief Qt implementation of the EditorLab Scenario Runner panel.

#include "ScenarioRunnerPanel.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace SagaEditorLab
{
namespace
{

[[nodiscard]] QString StepText(std::uint32_t executed, std::uint32_t total)
{
    return QStringLiteral("%1 / %2")
        .arg(static_cast<qulonglong>(executed))
        .arg(static_cast<qulonglong>(total));
}

[[nodiscard]] QString CountText(std::size_t count)
{
    return QString::number(static_cast<qulonglong>(count));
}

} // namespace

struct ScenarioRunnerPanel::Impl
{
    Impl()
    {
        widget = new QWidget();
        widget->setWindowTitle(QStringLiteral("EditorLab Scenario Runner"));
        widget->resize(1100, 700);

        auto* root = new QHBoxLayout(widget);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(10);

        scenarioList = new QListWidget(widget);
        scenarioList->setMinimumWidth(300);
        root->addWidget(scenarioList, 0);

        auto* right = new QVBoxLayout();
        right->setSpacing(8);
        root->addLayout(right, 1);

        titleLabel = new QLabel(widget);
        titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        right->addWidget(titleLabel);

        detailLabel = new QLabel(widget);
        detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        right->addWidget(detailLabel);

        runButton = new QPushButton(QStringLiteral("Run"), widget);
        right->addWidget(runButton, 0, Qt::AlignLeft);

        statusLabel = new QLabel(QStringLiteral("No run yet"), widget);
        statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        right->addWidget(statusLabel);

        diagnostics = new QTreeWidget(widget);
        diagnostics->setColumnCount(4);
        diagnostics->setHeaderLabels({
            QStringLiteral("Severity"),
            QStringLiteral("Step"),
            QStringLiteral("Label"),
            QStringLiteral("Message"),
        });
        diagnostics->setRootIsDecorated(false);
        diagnostics->setAlternatingRowColors(true);
        diagnostics->setUniformRowHeights(true);
        diagnostics->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        diagnostics->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        diagnostics->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        diagnostics->header()->setSectionResizeMode(3, QHeaderView::Stretch);
        right->addWidget(diagnostics, 1);

        snapshots = new QTreeWidget(widget);
        snapshots->setColumnCount(3);
        snapshots->setHeaderLabels({
            QStringLiteral("Snapshot"),
            QStringLiteral("Step"),
            QStringLiteral("Hash"),
        });
        snapshots->setRootIsDecorated(false);
        snapshots->setAlternatingRowColors(true);
        snapshots->setUniformRowHeights(true);
        snapshots->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        snapshots->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        snapshots->header()->setSectionResizeMode(2, QHeaderView::Stretch);
        right->addWidget(snapshots, 1);

        PopulateScenarios();
        RefreshSelection();
        RefreshResult();

        QObject::connect(
            scenarioList,
            &QListWidget::currentRowChanged,
            [this](int row)
            {
                const auto items = viewModel.GetScenarioItems();
                if (row >= 0 && static_cast<std::size_t>(row) < items.size())
                {
                    (void)viewModel.SelectScenario(items[static_cast<std::size_t>(row)].id);
                    RefreshSelection();
                    RefreshResult();
                }
            });

        QObject::connect(
            runButton,
            &QPushButton::clicked,
            [this]()
            {
                (void)viewModel.RunSelectedScenario();
                RefreshSelection();
                RefreshResult();
            });
    }

    void PopulateScenarios()
    {
        const auto items = viewModel.GetScenarioItems();
        for (const ScenarioRunnerScenarioItem& item : items)
        {
            auto* listItem = new QListWidgetItem(
                QString::fromStdString(item.name),
                scenarioList);
            listItem->setData(Qt::UserRole, QString::fromStdString(item.id));
        }

        if (!items.empty())
        {
            scenarioList->setCurrentRow(0);
        }
    }

    void RefreshSelection()
    {
        const ScenarioDefinition* selected = viewModel.GetSelectedScenario();
        if (!selected)
        {
            titleLabel->setText(QStringLiteral("No scenario selected"));
            detailLabel->setText({});
            runButton->setEnabled(false);
            return;
        }

        runButton->setEnabled(true);
        titleLabel->setText(QString::fromStdString(selected->name));
        detailLabel->setText(
            QStringLiteral("%1 | %2 steps")
                .arg(QString::fromStdString(selected->id))
                .arg(static_cast<qulonglong>(selected->steps.size())));
    }

    void RefreshResult()
    {
        diagnostics->clear();
        snapshots->clear();

        const ScenarioRunnerResultSummary summary = viewModel.GetResultSummary();
        if (!summary.hasResult)
        {
            statusLabel->setText(QStringLiteral("No run yet"));
            return;
        }

        statusLabel->setText(
            QStringLiteral("Verdict: %1 | Steps: %2 | Warnings: %3 | Errors: %4 | Diagnostics: %5 | Snapshots: %6")
                .arg(QString::fromStdString(summary.verdict),
                     StepText(summary.stepsExecuted, summary.stepsTotal),
                     CountText(summary.warningCount),
                     CountText(summary.errorCount),
                     CountText(summary.diagnosticCount),
                     CountText(summary.snapshotCount)));

        for (const ScenarioRunnerDiagnosticRow& row : viewModel.GetDiagnosticRows())
        {
            auto* item = new QTreeWidgetItem(diagnostics);
            item->setText(0, QString::fromStdString(row.severity));
            item->setText(1, QString::number(row.stepIndex));
            item->setText(2, QString::fromStdString(row.stepLabel));
            item->setText(3, QString::fromStdString(row.message));
        }

        for (const ScenarioRunnerSnapshotRow& row : viewModel.GetSnapshotRows())
        {
            auto* item = new QTreeWidgetItem(snapshots);
            item->setText(0, QString::fromStdString(row.label));
            item->setText(1, QString::number(row.stepIndex));
            item->setText(2, QString::fromStdString(row.sha256.empty() ? "-" : row.sha256));
        }
    }

    ScenarioRunnerPanelViewModel viewModel;
    QWidget* widget = nullptr;
    QListWidget* scenarioList = nullptr;
    QLabel* titleLabel = nullptr;
    QLabel* detailLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* runButton = nullptr;
    QTreeWidget* diagnostics = nullptr;
    QTreeWidget* snapshots = nullptr;
};

ScenarioRunnerPanel::ScenarioRunnerPanel()
    : m_impl(std::make_unique<Impl>())
{}

ScenarioRunnerPanel::~ScenarioRunnerPanel() = default;

QWidget* ScenarioRunnerPanel::Widget() const noexcept
{
    return m_impl->widget;
}

} // namespace SagaEditorLab
