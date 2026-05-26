/// @file WatchPanel.cpp
/// @brief Qt backend implementation of the visual scripting watch panel.

#include "SagaEditor/VisualScripting/Editor/WatchPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor::VisualScripting
{

struct WatchPanel::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaVisualScriptingWatchPanel");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Watch", widget);
        placeholder->setObjectName("SagaVisualScriptingWatchPanelPlaceholder");
        layout->addWidget(placeholder);
    }
};

WatchPanel::WatchPanel()
    : m_impl(std::make_unique<Impl>())
{}

WatchPanel::~WatchPanel() = default;

PanelId WatchPanel::GetPanelId() const
{
    return "saga.panel.visual_scripting.watch";
}

std::string WatchPanel::GetTitle() const
{
    return "Watch";
}

void* WatchPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void WatchPanel::OnInit() {}

void WatchPanel::OnShutdown() {}

} // namespace SagaEditor::VisualScripting
