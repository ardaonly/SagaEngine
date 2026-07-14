/// @file HierarchyPanel.cpp
/// @brief Qt backend implementation. The public header lives outside
///        UI/Qt/ (Panels/HierarchyPanel.h);
///        Qt headers are permitted only inside this folder. If you
///        need to use Qt for something new, add a sibling here.

#include "SagaEditor/Panels/HierarchyPanel.h"
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace SagaEditor
{

// ─── Qt widget implementation ─────────────────────────────────────────────────

struct HierarchyPanel::Impl
{
    QWidget*     widget    = nullptr;
    QLineEdit*   searchBox = nullptr;
    QTreeWidget* tree      = nullptr;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        searchBox = new QLineEdit(widget);
        searchBox->setPlaceholderText("Search entities…");
        layout->addWidget(searchBox);

        tree = new QTreeWidget(widget);
        tree->setHeaderHidden(true);
        tree->setUniformRowHeights(true);
        layout->addWidget(tree);

        QObject::connect(searchBox, &QLineEdit::textChanged,
            [this](const QString& text)
            {
                for (int i = 0; i < tree->topLevelItemCount(); ++i)
                {
                    auto* item = tree->topLevelItem(i);
                    item->setHidden(!text.isEmpty() &&
                                    !item->text(0).contains(text, Qt::CaseInsensitive));
                }
            });
    }

    void Populate()
    {
        tree->clear();
        auto* root = new QTreeWidgetItem(tree, QStringList{"World"});
        root->addChild(new QTreeWidgetItem(QStringList{"Zone_0_0"}));
        root->addChild(new QTreeWidgetItem(QStringList{"Zone_0_1"}));
        tree->expandAll();
    }
};

// ─── HierarchyPanel ───────────────────────────────────────────────────────────

HierarchyPanel::HierarchyPanel()
    : m_impl(std::make_unique<Impl>())
{}

HierarchyPanel::~HierarchyPanel() = default;

PanelId     HierarchyPanel::GetPanelId()      const { return "saga.panel.hierarchy"; }
std::string HierarchyPanel::GetTitle()        const { return "Hierarchy"; }
void*       HierarchyPanel::GetNativeWidget() const noexcept { return m_impl->widget; }

void HierarchyPanel::OnInit()     { m_impl->Populate(); }
void HierarchyPanel::OnShutdown() {}

void HierarchyPanel::SetSelectionManager(SelectionManager* /*mgr*/)
{
    // TODO: connect mgr's selection-changed callback → repopulate tree.
}

} // namespace SagaEditor
