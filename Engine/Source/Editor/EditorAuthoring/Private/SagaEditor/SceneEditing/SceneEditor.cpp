// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/SceneEditing/SceneEditor.h"
#include "SagaEditor/SceneEditing/SceneDocument.h"

#include <utility>

namespace SagaEditor
{

struct SceneEditor::Impl
{
    std::unique_ptr<SceneDocument> document;
    std::function<void()> onSceneChanged;
};

SceneEditor::SceneEditor() : m_impl(std::make_unique<Impl>()) {}
SceneEditor::~SceneEditor() = default;

bool SceneEditor::OpenScene(const std::string& path)
{
    auto document = std::make_unique<SceneDocument>(path);
    if (!document->Load()) return false;
    m_impl->document = std::move(document);
    if (m_impl->onSceneChanged) m_impl->onSceneChanged();
    return true;
}

void SceneEditor::CloseScene()
{
    if (!m_impl->document) return;
    m_impl->document.reset();
    if (m_impl->onSceneChanged) m_impl->onSceneChanged();
}

bool SceneEditor::SaveScene()
{
    if (!m_impl->document) return false;
    const auto status = m_impl->document->Save();
    return status == DocumentSaveStatus::Saved || status == DocumentSaveStatus::NotDirty;
}

bool SceneEditor::IsModified() const noexcept
{
    return m_impl->document && m_impl->document->Snapshot().dirty;
}

SceneDocument* SceneEditor::GetDocument() noexcept { return m_impl->document.get(); }
const SceneDocument* SceneEditor::GetDocument() const noexcept { return m_impl->document.get(); }

bool SceneEditor::RenameEntity(std::string_view entityId, std::string name)
{
    if (!m_impl->document || !m_impl->document->RenameEntity(entityId, std::move(name)))
        return false;
    if (m_impl->onSceneChanged) m_impl->onSceneChanged();
    return true;
}

void SceneEditor::SetOnSceneChanged(std::function<void()> cb) { m_impl->onSceneChanged = std::move(cb); }

} // namespace SagaEditor
