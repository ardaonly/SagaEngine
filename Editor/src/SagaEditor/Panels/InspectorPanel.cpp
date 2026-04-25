/// @file InspectorPanel.cpp
/// @brief Inspector panel — Qt widget lives entirely inside Impl.

#include "SagaEditor/Panels/InspectorPanel.h"
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace SagaEditor
{

// ─── Qt widget implementation ─────────────────────────────────────────────────

struct InspectorPanel::Impl
{
    QWidget*     widget      = nullptr;
    QScrollArea* scrollArea  = nullptr;
    QLabel*      placeholder = nullptr;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);

        scrollArea = new QScrollArea(widget);
        scrollArea->setWidgetResizable(true);
        layout->addWidget(scrollArea);

        auto* container = new QWidget();
        placeholder = new QLabel("Nothing selected.", container);
        placeholder->setAlignment(Qt::AlignCenter);

        auto* cl = new QVBoxLayout(container);
        cl->addWidget(placeholder);
        scrollArea->setWidget(container);
    }

    void Rebuild()
    {
        placeholder->setText("Nothing selected.");
        // TODO: instantiate property editors for selected entity.
    }
};

// ─── InspectorPanel ───────────────────────────────────────────────────────────

InspectorPanel::InspectorPanel()
    : m_impl(std::make_unique<Impl>())
{}

InspectorPanel::~InspectorPanel() = default;

PanelId     InspectorPanel::GetPanelId()      const { return "saga.panel.inspector"; }
std::string InspectorPanel::GetTitle()        const { return "Inspector"; }
void*       InspectorPanel::GetNativeWidget() const noexcept { return m_impl->widget; }

void InspectorPanel::OnInit()     {}
void InspectorPanel::OnShutdown() {}

void InspectorPanel::SetSelectionManager(SelectionManager* /*mgr*/)
{
    // TODO: connect mgr's on-selection-changed → m_impl->Rebuild().
}

} // namespace SagaEditor
