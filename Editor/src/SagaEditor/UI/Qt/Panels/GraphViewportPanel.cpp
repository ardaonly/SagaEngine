/// @file GraphViewportPanel.cpp
/// @brief Qt backend implementation of the graph viewport panel.

#include "SagaEditor/Panels/GraphViewportPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor
{

struct GraphViewportPanel::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaGraphViewportPanel");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Graph viewport", widget);
        placeholder->setObjectName("SagaGraphViewportPanelPlaceholder");
        layout->addWidget(placeholder);
    }
};

GraphViewportPanel::GraphViewportPanel()
    : m_impl(std::make_unique<Impl>())
{}

GraphViewportPanel::~GraphViewportPanel() = default;

PanelId GraphViewportPanel::GetPanelId() const
{
    return "saga.panel.graph";
}

std::string GraphViewportPanel::GetTitle() const
{
    return "Graph";
}

void* GraphViewportPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void GraphViewportPanel::OnInit() {}

void GraphViewportPanel::OnShutdown() {}

} // namespace SagaEditor
