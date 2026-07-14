/// @file ProjectBrowserPanel.cpp
/// @brief Qt backend implementation of the project browser panel.

#include "SagaEditor/Panels/ProjectBrowserPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor
{

struct ProjectBrowserPanel::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaProjectBrowserPanel");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Project Browser", widget);
        placeholder->setObjectName("SagaProjectBrowserPanelPlaceholder");
        layout->addWidget(placeholder);
    }
};

ProjectBrowserPanel::ProjectBrowserPanel()
    : m_impl(std::make_unique<Impl>())
{}

ProjectBrowserPanel::~ProjectBrowserPanel() = default;

PanelId ProjectBrowserPanel::GetPanelId() const
{
    return "saga.panel.project";
}

std::string ProjectBrowserPanel::GetTitle() const
{
    return "Project Browser";
}

void* ProjectBrowserPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void ProjectBrowserPanel::OnInit() {}

void ProjectBrowserPanel::OnShutdown() {}

} // namespace SagaEditor
