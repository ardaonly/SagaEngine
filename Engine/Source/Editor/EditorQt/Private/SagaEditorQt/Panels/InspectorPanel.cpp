/// @file InspectorPanel.cpp
/// @brief Qt backend implementation. The public header lives outside
///        UI/Qt/ (Panels/InspectorPanel.h);
///        Qt headers are permitted only inside this folder. If you
///        need to use Qt for something new, add a sibling here.

#include "SagaEditor/Panels/InspectorPanel.h"

#include "SagaEditor/InspectorEditing/ComponentEditorRegistry.h"
#include "SagaEditor/InspectorEditing/InspectorBinder.h"

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace SagaEditor
{

// ─── Qt Widget Implementation ─────────────────────────────────────────────────

struct InspectorPanel::Impl
{
    QWidget*     widget       = nullptr; ///< Outer panel widget (owned by Qt).
    QScrollArea* scrollArea   = nullptr; ///< Scrollable host for the editor list.
    QWidget*     editorHost   = nullptr; ///< Vertical stack of editor frames.
    QVBoxLayout* editorLayout = nullptr; ///< Layout that holds editor frames.
    QLabel*      placeholder  = nullptr; ///< "Nothing selected." fallback.

    ComponentEditorRegistry* registry        = nullptr; ///< Non-owning.
    InspectorBinder*         binder          = nullptr; ///< Non-owning.
    SelectionManager*        selectionMgr    = nullptr; ///< Non-owning.

    /// Currently nested editor panels. Owned by `m_panels` here so
    /// the inspector controls their lifetime regardless of Qt parent
    /// reparenting; the produced QWidget* is reparented into
    /// `editorHost` for layout but the IPanel ownership stays with us.
    std::vector<std::unique_ptr<IPanel>> editors;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);

        scrollArea = new QScrollArea(widget);
        scrollArea->setWidgetResizable(true);
        layout->addWidget(scrollArea);

        editorHost = new QWidget();
        editorLayout = new QVBoxLayout(editorHost);
        editorLayout->setContentsMargins(0, 0, 0, 0);
        editorLayout->setSpacing(2);
        editorLayout->setAlignment(Qt::AlignTop);

        placeholder = new QLabel("Nothing selected.", editorHost);
        placeholder->setAlignment(Qt::AlignCenter);
        editorLayout->addWidget(placeholder);

        scrollArea->setWidget(editorHost);
    }

    // ─── Editor List Management ───────────────────────────────────────────────

    void ShowPlaceholder(const QString& text)
    {
        // Drop owned editors first so their native widgets are deleted
        // before the placeholder takes the layout row over.
        editors.clear();
        placeholder->setText(text);
        placeholder->setVisible(true);
    }

    void RebuildFromTypes(const std::vector<std::type_index>& types)
    {
        editors.clear();

        if (!registry || types.empty())
        {
            placeholder->setText("Nothing selected.");
            placeholder->setVisible(true);
            return;
        }

        placeholder->setVisible(false);

        for (const auto& typeIdx : types)
        {
            auto editor = registry->Create(typeIdx);
            if (!editor)
            {
                continue;
            }

            auto* native = static_cast<QWidget*>(editor->GetNativeWidget());
            if (native)
            {
                editorLayout->addWidget(native);
            }
            editors.push_back(std::move(editor));
        }

        if (editors.empty())
        {
            placeholder->setText("No editors registered for this entity.");
            placeholder->setVisible(true);
        }
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
void InspectorPanel::OnShutdown() { m_impl->editors.clear(); }

// ─── Bindings ─────────────────────────────────────────────────────────────────

void InspectorPanel::SetSelectionManager(SelectionManager* mgr) noexcept
{
    m_impl->selectionMgr = mgr;
    // The selection-to-component-type bridge is wired by the host
    // because it needs ECS access; the panel itself just exposes the
    // sink via RefreshFromTypes.
}

void InspectorPanel::SetComponentEditorRegistry(ComponentEditorRegistry* registry) noexcept
{
    m_impl->registry = registry;
}

void InspectorPanel::SetInspectorBinder(InspectorBinder* binder) noexcept
{
    m_impl->binder = binder;
}

// ─── Rebuild ──────────────────────────────────────────────────────────────────

void InspectorPanel::RefreshFromTypes(const std::vector<std::type_index>& componentTypes)
{
    m_impl->RebuildFromTypes(componentTypes);
}

void InspectorPanel::ClearEditors()
{
    m_impl->ShowPlaceholder("Nothing selected.");
}

std::size_t InspectorPanel::EditorCount() const noexcept
{
    return m_impl->editors.size();
}

} // namespace SagaEditor
