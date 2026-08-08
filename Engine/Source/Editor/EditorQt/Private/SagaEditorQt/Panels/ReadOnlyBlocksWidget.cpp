// SPDX-License-Identifier: MPL-2.0

#include "SagaEditorQt/Panels/ReadOnlyBlocksWidget.h"

#include "SagaEditor/VisualBlocks/ControlledBlockEditController.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace SagaEditor
{
ReadOnlyBlocksWidget::ReadOnlyBlocksWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    status_ = new QLabel(QStringLiteral("Save and analyze to view blocks"), this);
    status_->setAccessibleName(QStringLiteral("Blocks projection status"));
    tree_ = new QTreeWidget(this);
    tree_->setAccessibleName(QStringLiteral("Controlled script blocks"));
    tree_->setHeaderLabels({QStringLiteral("Projection"), QStringLiteral("Package origin")});
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    auto* energyCore = new QWidget(this);
    energyCore->setObjectName(QStringLiteral("EnergyCoreControlledFields"));
    energyCore->setAccessibleName(QStringLiteral("Energy Core controlled fields"));
    auto* form = new QFormLayout(energyCore);
    score_ = new QSpinBox(energyCore);
    score_->setAccessibleName(QStringLiteral("Energy Core score"));
    score_->setRange(1, std::numeric_limits<int>::max());
    score_->setValue(10);
    respawn_ = new QDoubleSpinBox(energyCore);
    respawn_->setAccessibleName(QStringLiteral("Energy Core respawn seconds"));
    respawn_->setRange(0.0, 86400.0);
    respawn_->setDecimals(6);
    respawn_->setValue(2.0);
    previewScore_ = new QPushButton(QStringLiteral("Preview score change"), energyCore);
    previewRespawn_ = new QPushButton(QStringLiteral("Preview respawn change"), energyCore);
    form->addRow(QStringLiteral("Energy Core score"), score_);
    form->addRow(QStringLiteral("Respawn seconds"), respawn_);
    form->addRow(previewScore_);
    form->addRow(previewRespawn_);
    layout->addWidget(status_);
    layout->addWidget(energyCore);
    layout->addWidget(tree_, 1);
    connect(previewScore_, &QPushButton::clicked, this, [this]
    {
        PreviewControlledField(
            "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer",
            std::to_string(score_->value()));
    });
    connect(previewRespawn_, &QPushButton::clicked, this, [this]
    {
        PreviewControlledField(
            "field://ArenaRush.ArenaRushGameMode/EnergyCoreRespawnSeconds/initializer",
            QString::number(respawn_->value(), 'g', 15).toStdString());
    });
    RefreshControlledState();
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item)
    {
        const auto index = item->data(0, Qt::UserRole).toInt();
        if (index >= 0 && static_cast<std::size_t>(index) < spans_.size() && sourceActivated_)
            sourceActivated_(spans_[static_cast<std::size_t>(index)]);
    });
}

ReadOnlyBlocksWidget::~ReadOnlyBlocksWidget()
{
    if (planWatcher_ && planWatcher_->isRunning()) planWatcher_->waitForFinished();
}

void ReadOnlyBlocksWidget::SetStatus(const QString& status)
{ status_->setText(status); }

void ReadOnlyBlocksWidget::SetSourceActivated(
    std::function<void(const VisualBlocks::ProjectionSourceSpan&)> callback)
{ sourceActivated_ = std::move(callback); }

void ReadOnlyBlocksWidget::SetControlledEditing(
    ControlledBlockEditController* controller, CSharpSourceDocument* document)
{
    if (planWatcher_ && planWatcher_->isRunning()) planWatcher_->waitForFinished();
    controlledController_ = controller;
    controlledDocument_ = document;
    RefreshControlledState();
}

void ReadOnlyBlocksWidget::RefreshControlledState()
{
    const bool supported = controlledController_ && controlledDocument_ &&
        controlledDocument_->Snapshot().canonicalSourcePath.filename() ==
            "ArenaRushGameMode.cs";
    const bool scoreSupported = supported && projection_ &&
        std::ranges::any_of(projection_->EditableFields(), [](const auto& field)
        {
            return field.fieldId ==
                "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer";
        });
    const bool respawnSupported = supported && projection_ &&
        std::ranges::any_of(projection_->EditableFields(), [](const auto& field)
        {
            return field.fieldId ==
                "field://ArenaRush.ArenaRushGameMode/EnergyCoreRespawnSeconds/initializer";
        });
    score_->setEnabled(scoreSupported);
    previewScore_->setEnabled(scoreSupported);
    respawn_->setEnabled(respawnSupported);
    previewRespawn_->setEnabled(respawnSupported);
}

void ReadOnlyBlocksWidget::PreviewControlledField(
    const std::string& fieldId, const std::string& value)
{
    if (!controlledController_ || !controlledDocument_ ||
        (planWatcher_ && planWatcher_->isRunning())) return;
    SetStatus(QStringLiteral("Planning controlled source edit..."));
    previewScore_->setEnabled(false);
    previewRespawn_->setEnabled(false);
    auto* controller = controlledController_;
    auto* document = controlledDocument_;
    planWatcher_ = new QFutureWatcher<ControlledBlockEditPlan>(this);
    connect(planWatcher_, &QFutureWatcher<ControlledBlockEditPlan>::finished,
        this, [this, controller, document]
    {
        const auto plan = planWatcher_->result();
        planWatcher_->deleteLater();
        planWatcher_ = nullptr;
        if (controller != controlledController_ || document != controlledDocument_)
        {
            RefreshControlledState();
            return;
        }
        auto* review = new QDialog(this);
        review->setAttribute(Qt::WA_DeleteOnClose);
        review->setObjectName(QStringLiteral("ControlledEditReviewDialog"));
        review->setWindowTitle(QStringLiteral("Review controlled C# change"));
        auto* layout = new QVBoxLayout(review);
        auto* diff = new QPlainTextEdit(review);
        diff->setAccessibleName(QStringLiteral("Controlled C# diff"));
        diff->setReadOnly(true);
        diff->setPlainText(QString::fromStdString(plan.minimalDiff));
        layout->addWidget(diff);
        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Apply | QDialogButtonBox::Cancel, review);
        buttons->button(QDialogButtonBox::Apply)->setEnabled(plan.accepted);
        connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            review, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, review, &QDialog::reject);
        layout->addWidget(buttons);
        connect(review, &QDialog::accepted, this,
            [this, controller, document, plan]
        {
            const bool applied = controller->Apply(*document, plan);
            SetStatus(applied ? QStringLiteral("Controlled source edit applied") :
                QString::fromStdString(controller->Diagnostic()));
            RefreshControlledState();
        });
        connect(review, &QDialog::rejected, this, [this, controller]
        {
            controller->Cancel();
            RefreshControlledState();
        });
        review->open();
    });
    planWatcher_->setFuture(QtConcurrent::run(
        [controller, document, fieldId, value]
        { return controller->Plan(*document, fieldId, value); }));
}

void ReadOnlyBlocksWidget::Present(
    std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection)
{
    projection_ = std::move(projection);
    spans_.clear();
    tree_->clear();
    if (!projection_)
    {
        RefreshControlledState();
        SetStatus(QStringLiteral("No current projection"));
        return;
    }
    for (const auto& field : projection_->EditableFields())
    {
        if (field.fieldId.ends_with("/EnergyCoreScore/initializer"))
            score_->setValue(QString::fromStdString(field.value).toInt());
        else if (field.fieldId.ends_with("/EnergyCoreRespawnSeconds/initializer"))
            respawn_->setValue(QString::fromStdString(field.value).toDouble());
    }
    for (const auto& card : projection_->Roots()) AddCard(card, nullptr);
    for (const auto& card : projection_->OpaqueRegions()) AddCard(card, nullptr);
    tree_->expandAll();
    RefreshControlledState();
    SetStatus(QStringLiteral("Current controlled projection - revision %1 - %2")
        .arg(QString::fromStdString(projection_->SourceRevision()),
            QString::fromStdString(projection_->PackageGenerationId())));
}

void ReadOnlyBlocksWidget::PresentSnapshot(const ScriptProjectionSnapshot& snapshot)
{
    if (snapshot.state == ScriptProjectionState::Current && snapshot.projection)
    {
        Present(snapshot.projection);
        return;
    }

    Present(nullptr);
    switch (snapshot.state)
    {
    case ScriptProjectionState::NoDocument:
        SetStatus(QStringLiteral("No document"));
        break;
    case ScriptProjectionState::DirtyNeedsSave:
        SetStatus(QStringLiteral("Save changes before analysis"));
        break;
    case ScriptProjectionState::Analyzing:
        SetStatus(QStringLiteral("Analyzing..."));
        break;
    case ScriptProjectionState::Stale:
        SetStatus(QStringLiteral("Stale projection rejected"));
        break;
    case ScriptProjectionState::Failed:
        SetStatus(snapshot.diagnostic.empty()
            ? QStringLiteral("Projection failed")
            : QStringLiteral("Projection failed: %1")
                .arg(QString::fromStdString(snapshot.diagnostic)));
        break;
    case ScriptProjectionState::Cancelled:
        SetStatus(QStringLiteral("Analysis cancelled"));
        break;
    case ScriptProjectionState::ProjectClosed:
        SetStatus(QStringLiteral("Project closed"));
        break;
    case ScriptProjectionState::PackageGenerationChanged:
        SetStatus(QStringLiteral("Package generation changed"));
        break;
    case ScriptProjectionState::Current:
        SetStatus(QStringLiteral("Current projection unavailable"));
        break;
    }
}

void ReadOnlyBlocksWidget::AddCard(const VisualBlocks::ProjectionCard& card, void* parentItem)
{
    auto* item = parentItem
        ? new QTreeWidgetItem(static_cast<QTreeWidgetItem*>(parentItem))
        : new QTreeWidgetItem(tree_);
    item->setText(0, QString::fromStdString(card.DisplayName()));
    item->setText(1, QString::fromStdString(card.PackageOrigin()));
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    const auto index = static_cast<int>(spans_.size());
    spans_.push_back(card.Source());
    item->setData(0, Qt::UserRole, index);
    for (const auto& child : card.Children()) AddCard(child, item);
}
}
