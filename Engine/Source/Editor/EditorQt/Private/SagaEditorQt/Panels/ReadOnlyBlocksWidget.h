// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Scripting/ScriptProjectionSession.h"
#include "SagaEditor/VisualBlocks/ReadOnlyScriptProjection.h"

#include <QWidget>

#include <functional>
#include <memory>
#include <vector>

class QLabel;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QTreeWidget;
template <typename T> class QFutureWatcher;

namespace SagaEditor
{
class ControlledBlockEditController;
struct ControlledBlockEditPlan;
class CSharpSourceDocument;
class ReadOnlyBlocksWidget final : public QWidget
{
public:
    explicit ReadOnlyBlocksWidget(QWidget* parent = nullptr);
    ~ReadOnlyBlocksWidget() override;
    void Present(std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection);
    void PresentSnapshot(const ScriptProjectionSnapshot& snapshot);
    void SetStatus(const QString& status);
    void SetSourceActivated(std::function<void(const VisualBlocks::ProjectionSourceSpan&)> callback);
    void SetControlledEditing(ControlledBlockEditController* controller,
        CSharpSourceDocument* document);

private:
    void AddCard(const VisualBlocks::ProjectionCard& card, void* parentItem);
    void PreviewControlledField(const std::string& fieldId, const std::string& value);
    void RefreshControlledState();

    QLabel* status_ = nullptr;
    QSpinBox* score_ = nullptr;
    QDoubleSpinBox* respawn_ = nullptr;
    QPushButton* previewScore_ = nullptr;
    QPushButton* previewRespawn_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection_;
    std::vector<VisualBlocks::ProjectionSourceSpan> spans_;
    std::function<void(const VisualBlocks::ProjectionSourceSpan&)> sourceActivated_;
    ControlledBlockEditController* controlledController_ = nullptr;
    CSharpSourceDocument* controlledDocument_ = nullptr;
    QFutureWatcher<ControlledBlockEditPlan>* planWatcher_ = nullptr;
};
}
