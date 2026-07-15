/// @file NodePalette.cpp
/// @brief Qt backend implementation of the visual scripting node palette panel.

#include "SagaEditor/VisualBlocks/Editor/NodePalette.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor::VisualBlocks
{

struct NodePalette::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaVisualScriptingNodePalette");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Node Palette", widget);
        placeholder->setObjectName("SagaVisualScriptingNodePalettePlaceholder");
        layout->addWidget(placeholder);
    }
};

NodePalette::NodePalette()
    : m_impl(std::make_unique<Impl>())
{}

NodePalette::~NodePalette() = default;

PanelId NodePalette::GetPanelId() const
{
    return "saga.panel.visual_scripting.node_palette";
}

std::string NodePalette::GetTitle() const
{
    return "Node Palette";
}

void* NodePalette::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void NodePalette::OnInit() {}

void NodePalette::OnShutdown() {}

} // namespace SagaEditor::VisualBlocks
