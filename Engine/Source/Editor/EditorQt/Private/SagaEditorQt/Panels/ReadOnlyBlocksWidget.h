// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/VisualBlocks/ReadOnlyScriptProjection.h"

#include <QWidget>

#include <functional>
#include <memory>
#include <vector>

class QLabel;
class QTreeWidget;

namespace SagaEditor
{
class ReadOnlyBlocksWidget final : public QWidget
{
public:
    explicit ReadOnlyBlocksWidget(QWidget* parent = nullptr);
    void Present(std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection);
    void SetStatus(const QString& status);
    void SetSourceActivated(std::function<void(const VisualBlocks::ProjectionSourceSpan&)> callback);

private:
    void AddCard(const VisualBlocks::ProjectionCard& card, void* parentItem);

    QLabel* status_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection_;
    std::vector<VisualBlocks::ProjectionSourceSpan> spans_;
    std::function<void(const VisualBlocks::ProjectionSourceSpan&)> sourceActivated_;
};
}
