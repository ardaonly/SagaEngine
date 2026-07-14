/// @file QtPanel.h
/// @brief Qt-backed base class for all editor panels — bridges IPanel and QWidget.

#pragma once

#include "SagaEditor/Panels/IPanel.h"
#include <QWidget>

namespace SagaEditor
{

// ─── Qt Panel ─────────────────────────────────────────────────────────────────

/// Concrete base that satisfies both IPanel (pure-C++ interface) and QWidget
/// (Qt widget hierarchy). All editor panels that target the Qt backend should
/// inherit from QtPanel rather than IPanel directly.
///
/// Inheritance order is important: QWidget must come before IPanel so that
/// QObject's vtable is resolved first by moc.
class QtPanel : public QWidget, public IPanel
{
    Q_OBJECT

public:
    explicit QtPanel(QWidget* parent = nullptr);
    ~QtPanel() override = default;

    /// Returns `this` as a QWidget* cast to void* — passed to
    /// IUIMainWindow::DockPanel without exposing Qt types to the docking layer.
    [[nodiscard]] void* GetNativeWidget() const noexcept override;
};

} // namespace SagaEditor
