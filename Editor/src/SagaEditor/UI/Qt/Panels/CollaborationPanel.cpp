/// @file CollaborationPanel.cpp
/// @brief Qt backend implementation of the collaboration panel.

#include "SagaEditor/Panels/CollaborationPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor
{

struct CollaborationPanel::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaCollaborationPanel");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Collaboration", widget);
        placeholder->setObjectName("SagaCollaborationPanelPlaceholder");
        layout->addWidget(placeholder);
    }
};

CollaborationPanel::CollaborationPanel()
    : m_impl(std::make_unique<Impl>())
{}

CollaborationPanel::~CollaborationPanel() = default;

PanelId CollaborationPanel::GetPanelId() const
{
    return "saga.panel.collaboration";
}

std::string CollaborationPanel::GetTitle() const
{
    return "Collaboration";
}

void* CollaborationPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void CollaborationPanel::OnInit() {}

void CollaborationPanel::OnShutdown() {}

} // namespace SagaEditor
