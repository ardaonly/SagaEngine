/// @file BreakpointPanel.cpp
/// @brief Qt backend implementation of the visual scripting breakpoint panel.

#include "SagaEditor/VisualScripting/Editor/BreakpointPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor::VisualScripting
{

struct BreakpointPanel::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaVisualScriptingBreakpointPanel");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Breakpoints", widget);
        placeholder->setObjectName("SagaVisualScriptingBreakpointPanelPlaceholder");
        layout->addWidget(placeholder);
    }
};

BreakpointPanel::BreakpointPanel()
    : m_impl(std::make_unique<Impl>())
{}

BreakpointPanel::~BreakpointPanel() = default;

PanelId BreakpointPanel::GetPanelId() const
{
    return "saga.panel.visual_scripting.breakpoints";
}

std::string BreakpointPanel::GetTitle() const
{
    return "Breakpoints";
}

void* BreakpointPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void BreakpointPanel::OnInit() {}

void BreakpointPanel::OnShutdown() {}

} // namespace SagaEditor::VisualScripting
