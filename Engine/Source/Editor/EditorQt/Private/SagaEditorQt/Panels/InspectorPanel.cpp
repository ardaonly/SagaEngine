// SPDX-License-Identifier: MPL-2.0
/// @file InspectorPanel.cpp
/// @brief Qt backend implementation. The public header lives outside
///        UI/Qt/ (Panels/InspectorPanel.h);
///        Qt headers are permitted only inside this folder. If you
///        need to use Qt for something new, add a sibling here.

#include "SagaEditor/Panels/InspectorPanel.h"

#include "SagaEditor/InspectorEditing/ComponentEditorRegistry.h"
#include "SagaEditor/InspectorEditing/InspectorBinder.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/SceneEditing/SceneDocumentTransaction.h"
#include "SagaEditor/SceneEditing/SceneSelectionIdentity.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QTimer>
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
    QGroupBox* sceneGroup = nullptr;
    QFormLayout* sceneForm = nullptr;

    ComponentEditorRegistry* registry        = nullptr; ///< Non-owning.
    InspectorBinder*         binder          = nullptr; ///< Non-owning.
    SelectionManager*        selectionMgr    = nullptr; ///< Non-owning.
    EditorDocumentWorkspace* workspace = nullptr;
    EditorDocumentWorkspace::ChangeSubscription workspaceSubscription =
        EditorDocumentWorkspace::InvalidChangeSubscription;
    UndoRedoStack* journal = nullptr;
    SelectionSubscription selectionSubscription = kInvalidSelectionSubscription;
    SceneDocument* observedScene = nullptr;
    SceneDocument::ChangeSubscription sceneSubscription = SceneDocument::InvalidChangeSubscription;
    std::string selectedEntityId;
    std::vector<QDoubleSpinBox*> transformEditors;

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

        sceneGroup = new QGroupBox("Scene Entity", editorHost);
        sceneForm = new QFormLayout(sceneGroup);
        sceneGroup->setVisible(false);
        editorLayout->addWidget(sceneGroup);

        placeholder = new QLabel("Nothing selected.", editorHost);
        placeholder->setAlignment(Qt::AlignCenter);
        editorLayout->addWidget(placeholder);

        scrollArea->setWidget(editorHost);
    }

    void ClearSceneForm()
    {
        while (sceneForm->count() > 0)
        {
            auto* item = sceneForm->takeAt(0);
            delete item->widget();
            delete item;
        }
        transformEditors.clear();
        selectedEntityId.clear();
        sceneGroup->setVisible(false);
    }

    const SceneEntityReadModel* ResolveEntity(SelectableId selected) const
    {
        if (!workspace || !workspace->ActiveScene()) return nullptr;
        const auto& scene = *workspace->ActiveScene();
        for (const auto& entity : scene.Entities())
            if (SceneSelectionId(scene.SceneId(), entity.entityId) == selected) return &entity;
        return nullptr;
    }

    void CommitTransform()
    {
        if (!workspace || !workspace->ActiveScene() || !journal ||
            selectedEntityId.empty() || transformEditors.size() != 10) return;
        SceneTransformReadModel transform;
        for (std::size_t index = 0; index < 3; ++index)
            transform.position[index] = transformEditors[index]->value();
        for (std::size_t index = 0; index < 4; ++index)
            transform.rotation[index] = transformEditors[index + 3]->value();
        for (std::size_t index = 0; index < 3; ++index)
            transform.scale[index] = transformEditors[index + 7]->value();
        SceneDocumentTransaction transaction(*workspace->ActiveScene(), *journal);
        (void)transaction.SetTransform(selectedEntityId, transform);
    }

    void RebuildSceneEntity(SelectableId selected)
    {
        ClearSceneForm();
        const auto* entity = ResolveEntity(selected);
        if (!entity) return;
        selectedEntityId = entity->entityId;
        sceneGroup->setVisible(true);
        auto* identity = new QLabel(QString::fromStdString(entity->entityId), sceneGroup);
        identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
        sceneForm->addRow("Entity ID", identity);
        auto* name = new QLineEdit(QString::fromStdString(entity->name), sceneGroup);
        sceneForm->addRow("Name", name);
        QObject::connect(name, &QLineEdit::editingFinished, [this, name]
        {
            if (!workspace || !workspace->ActiveScene() || !journal) return;
            SceneDocumentTransaction transaction(*workspace->ActiveScene(), *journal);
            (void)transaction.RenameEntity(selectedEntityId, name->text().toStdString());
        });
        if (entity->transform)
        {
            const auto addValue = [this](const QString& label, double value)
            {
                auto* editor = new QDoubleSpinBox(sceneGroup);
                editor->setRange(-1000000.0, 1000000.0);
                editor->setDecimals(6);
                editor->setValue(value);
                sceneForm->addRow(label, editor);
                transformEditors.push_back(editor);
                QObject::connect(editor, &QDoubleSpinBox::editingFinished,
                    [this] { CommitTransform(); });
            };
            for (std::size_t i = 0; i < 3; ++i) addValue(QString("Position %1").arg(i), entity->transform->position[i]);
            for (std::size_t i = 0; i < 4; ++i) addValue(QString("Rotation %1").arg(i), entity->transform->rotation[i]);
            for (std::size_t i = 0; i < 3; ++i) addValue(QString("Scale %1").arg(i), entity->transform->scale[i]);
        }
        for (const auto& component : entity->components)
            sceneForm->addRow(QString::fromStdString(component.type),
                              new QLabel(QString::fromStdString(component.canonicalProperties), sceneGroup));
    }

    void DetachSelection()
    {
        if (selectionMgr && selectionSubscription != kInvalidSelectionSubscription)
            selectionMgr->Unsubscribe(selectionSubscription);
        selectionSubscription = kInvalidSelectionSubscription;
        selectionMgr = nullptr;
    }

    void DetachScene()
    {
        if (observedScene && sceneSubscription != SceneDocument::InvalidChangeSubscription)
            observedScene->UnsubscribeChanges(sceneSubscription);
        observedScene = nullptr;
        sceneSubscription = SceneDocument::InvalidChangeSubscription;
    }

    void RefreshWorkspace()
    {
        DetachScene();
        if (workspace && workspace->ActiveScene())
        {
            observedScene = workspace->ActiveScene();
            sceneSubscription = observedScene->SubscribeChanges([this]
            {
                QTimer::singleShot(0, widget, [this]
                {
                    RebuildSceneEntity(selectionMgr ? selectionMgr->GetPrimary() : kInvalidSelectableId);
                });
            });
        }
        RebuildSceneEntity(selectionMgr ? selectionMgr->GetPrimary() : kInvalidSelectableId);
    }

    void DetachWorkspace()
    {
        DetachScene();
        if (workspace && workspaceSubscription != EditorDocumentWorkspace::InvalidChangeSubscription)
            workspace->UnsubscribeChanges(workspaceSubscription);
        workspaceSubscription = EditorDocumentWorkspace::InvalidChangeSubscription;
        workspace = nullptr;
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

InspectorPanel::~InspectorPanel() { m_impl->DetachSelection(); m_impl->DetachWorkspace(); }

PanelId     InspectorPanel::GetPanelId()      const { return "saga.panel.inspector"; }
std::string InspectorPanel::GetTitle()        const { return "Inspector"; }
void*       InspectorPanel::GetNativeWidget() const noexcept { return m_impl->widget; }

void InspectorPanel::OnInit()     {}
void InspectorPanel::OnShutdown()
{
    m_impl->DetachSelection(); m_impl->DetachWorkspace(); m_impl->editors.clear();
}

// ─── Bindings ─────────────────────────────────────────────────────────────────

void InspectorPanel::SetSelectionManager(SelectionManager* mgr) noexcept
{
    m_impl->DetachSelection();
    m_impl->selectionMgr = mgr;
    if (mgr)
    {
        m_impl->selectionSubscription = mgr->Subscribe([impl = m_impl.get()](const auto& selection)
        { impl->RebuildSceneEntity(selection.empty() ? kInvalidSelectableId : selection.front()); });
        m_impl->RebuildSceneEntity(mgr->GetPrimary());
    }
}

void InspectorPanel::SetComponentEditorRegistry(ComponentEditorRegistry* registry) noexcept
{
    m_impl->registry = registry;
}

void InspectorPanel::SetInspectorBinder(InspectorBinder* binder) noexcept
{
    m_impl->binder = binder;
}

void InspectorPanel::SetDocumentWorkspace(EditorDocumentWorkspace* workspace) noexcept
{
    m_impl->DetachWorkspace();
    m_impl->workspace = workspace;
    if (workspace)
    {
        m_impl->workspaceSubscription = workspace->SubscribeChanges(
            [impl = m_impl.get()] { impl->RefreshWorkspace(); });
    }
    m_impl->RefreshWorkspace();
}

void InspectorPanel::SetUndoRedoStack(UndoRedoStack* journal) noexcept
{
    m_impl->journal = journal;
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
