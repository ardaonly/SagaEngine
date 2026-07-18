// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/SceneEditing/SceneDocumentTransaction.h"

#include "SagaEditor/Commands/UndoRedoStack.h"

#include <cmath>

namespace SagaEditor
{
namespace
{
class SceneMutationAction final : public IEditorAction
{
public:
    SceneMutationAction(std::string label, std::function<bool()> execute,
                        SceneDocument& document)
        : label_(std::move(label)), execute_(std::move(execute)), document_(document) {}
    void Execute() override
    {
        if (executedOnce_) (void)document_.Redo();
        else { succeeded_ = execute_(); executedOnce_ = true; }
    }
    void Undo() override { if (succeeded_) (void)document_.Undo(); }
    std::string GetLabel() const override { return label_; }
private:
    std::string label_;
    std::function<bool()> execute_;
    SceneDocument& document_;
    bool executedOnce_ = false;
    bool succeeded_ = false;
};
}

SceneDocumentTransaction::SceneDocumentTransaction(SceneDocument& document,
                                                   UndoRedoStack& journal)
    : document_(document), journal_(journal) {}

bool SceneDocumentTransaction::RenameEntity(std::string entityId, std::string name)
{
    if (name.empty() || !document_.FindEntity(entityId)) return false;
    journal_.Push(std::make_unique<SceneMutationAction>("Rename Entity",
        [this, entityId = std::move(entityId), name = std::move(name)]
        { return document_.RenameEntity(entityId, name); }, document_));
    return true;
}

bool SceneDocumentTransaction::SetTransform(std::string entityId,
                                            SceneTransformReadModel transform)
{
    const auto* entity = document_.FindEntity(entityId);
    if (!entity || !entity->transform) return false;
    for (const auto value : transform.position) if (!std::isfinite(value)) return false;
    for (const auto value : transform.rotation) if (!std::isfinite(value)) return false;
    for (const auto value : transform.scale) if (!std::isfinite(value)) return false;
    journal_.Push(std::make_unique<SceneMutationAction>("Edit Transform",
        [this, entityId = std::move(entityId), transform]
        { return document_.SetTransform(entityId, transform); }, document_));
    return true;
}

} // namespace SagaEditor
