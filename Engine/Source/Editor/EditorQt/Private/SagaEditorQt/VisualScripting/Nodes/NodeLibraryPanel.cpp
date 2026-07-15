/// @file NodeLibraryPanel.cpp
/// @brief Qt backend implementation of the visual scripting node library panel.

#include "SagaEditor/VisualBlocks/Nodes/NodeLibraryPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor::VisualBlocks
{

struct NodeLibraryPanel::Impl
{
    NodeCategoryBrowser& browser;
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    explicit Impl(NodeCategoryBrowser& nodeBrowser)
        : browser(nodeBrowser)
    {
        widget = new QWidget();
        widget->setObjectName("SagaVisualScriptingNodeLibraryPanel");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Node Library", widget);
        placeholder->setObjectName("SagaVisualScriptingNodeLibraryPanelPlaceholder");
        layout->addWidget(placeholder);
    }
};

NodeLibraryPanel::NodeLibraryPanel(NodeCategoryBrowser& browser)
    : m_impl(std::make_unique<Impl>(browser))
{}

NodeLibraryPanel::~NodeLibraryPanel() = default;

PanelId NodeLibraryPanel::GetPanelId() const
{
    return "saga.panel.visual_scripting.node_library";
}

std::string NodeLibraryPanel::GetTitle() const
{
    return "Node Library";
}

void* NodeLibraryPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void NodeLibraryPanel::OnInit() {}

void NodeLibraryPanel::OnShutdown() {}

} // namespace SagaEditor::VisualBlocks
