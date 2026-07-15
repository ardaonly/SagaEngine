/// @file GraphDebuggerView.cpp
/// @brief Qt backend implementation of the visual scripting graph debugger view.

#include "SagaEditor/VisualBlocks/Debugger/GraphDebuggerView.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor::VisualBlocks
{

struct GraphDebuggerView::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaVisualScriptingGraphDebuggerView");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Graph Debugger", widget);
        placeholder->setObjectName("SagaVisualScriptingGraphDebuggerViewPlaceholder");
        layout->addWidget(placeholder);
    }
};

GraphDebuggerView::GraphDebuggerView()
    : m_impl(std::make_unique<Impl>())
{}

GraphDebuggerView::~GraphDebuggerView() = default;

PanelId GraphDebuggerView::GetPanelId() const
{
    return "saga.panel.visual_scripting.graph_debugger";
}

std::string GraphDebuggerView::GetTitle() const
{
    return "Graph Debugger";
}

void* GraphDebuggerView::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void GraphDebuggerView::OnInit() {}

void GraphDebuggerView::OnShutdown() {}

} // namespace SagaEditor::VisualBlocks
