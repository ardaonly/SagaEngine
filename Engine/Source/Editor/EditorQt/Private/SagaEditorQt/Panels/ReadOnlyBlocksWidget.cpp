// SPDX-License-Identifier: MPL-2.0

#include "SagaEditorQt/Panels/ReadOnlyBlocksWidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace SagaEditor
{
ReadOnlyBlocksWidget::ReadOnlyBlocksWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    status_ = new QLabel(QStringLiteral("Save and analyze to view blocks"), this);
    status_->setAccessibleName(QStringLiteral("Blocks projection status"));
    tree_ = new QTreeWidget(this);
    tree_->setAccessibleName(QStringLiteral("Read-only script blocks"));
    tree_->setHeaderLabels({QStringLiteral("Projection"), QStringLiteral("Package origin")});
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(status_);
    layout->addWidget(tree_, 1);
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item)
    {
        const auto index = item->data(0, Qt::UserRole).toInt();
        if (index >= 0 && static_cast<std::size_t>(index) < spans_.size() && sourceActivated_)
            sourceActivated_(spans_[static_cast<std::size_t>(index)]);
    });
}

void ReadOnlyBlocksWidget::SetStatus(const QString& status)
{ status_->setText(status); }

void ReadOnlyBlocksWidget::SetSourceActivated(
    std::function<void(const VisualBlocks::ProjectionSourceSpan&)> callback)
{ sourceActivated_ = std::move(callback); }

void ReadOnlyBlocksWidget::Present(
    std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection)
{
    projection_ = std::move(projection);
    spans_.clear();
    tree_->clear();
    if (!projection_)
    {
        SetStatus(QStringLiteral("No current projection"));
        return;
    }
    for (const auto& card : projection_->Roots()) AddCard(card, nullptr);
    for (const auto& card : projection_->OpaqueRegions()) AddCard(card, nullptr);
    tree_->expandAll();
    SetStatus(QStringLiteral("Current • read-only • %1")
        .arg(QString::fromStdString(projection_->PackageGenerationId())));
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
        SetStatus(QStringLiteral("Analyzing…"));
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
