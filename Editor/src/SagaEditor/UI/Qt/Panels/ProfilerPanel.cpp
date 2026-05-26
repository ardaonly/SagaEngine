/// @file ProfilerPanel.cpp
/// @brief Qt backend implementation of the profiler panel.

#include "SagaEditor/Panels/ProfilerPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace SagaEditor
{

struct ProfilerPanel::Impl
{
    QWidget* widget = nullptr; ///< Owned by the Qt main-window dock host.

    Impl()
    {
        widget = new QWidget();
        widget->setObjectName("SagaProfilerPanel");

        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* placeholder = new QLabel("Profiler", widget);
        placeholder->setObjectName("SagaProfilerPanelPlaceholder");
        layout->addWidget(placeholder);
    }
};

ProfilerPanel::ProfilerPanel()
    : m_impl(std::make_unique<Impl>())
{}

ProfilerPanel::~ProfilerPanel() = default;

PanelId ProfilerPanel::GetPanelId() const
{
    return "saga.panel.profiler";
}

std::string ProfilerPanel::GetTitle() const
{
    return "Profiler";
}

void* ProfilerPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void ProfilerPanel::OnInit() {}

void ProfilerPanel::OnShutdown() {}

} // namespace SagaEditor
