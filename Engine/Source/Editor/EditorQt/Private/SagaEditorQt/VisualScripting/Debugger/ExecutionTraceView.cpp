/// @file ExecutionTraceView.cpp
/// @brief Qt backend implementation of the visual scripting execution trace view.

#include "SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor::VisualScripting
{

struct ExecutionTraceView::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaVisualScriptingExecutionTraceView");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Execution Trace", widget);
        placeholder->setObjectName("SagaVisualScriptingExecutionTraceViewPlaceholder");
        layout->addWidget(placeholder);
    }
};

ExecutionTraceView::ExecutionTraceView()
    : m_impl(std::make_unique<Impl>())
{}

ExecutionTraceView::~ExecutionTraceView() = default;

PanelId ExecutionTraceView::GetPanelId() const
{
    return "saga.panel.visual_scripting.execution_trace";
}

std::string ExecutionTraceView::GetTitle() const
{
    return "Execution Trace";
}

void* ExecutionTraceView::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void ExecutionTraceView::OnInit() {}

void ExecutionTraceView::OnShutdown() {}

} // namespace SagaEditor::VisualScripting
