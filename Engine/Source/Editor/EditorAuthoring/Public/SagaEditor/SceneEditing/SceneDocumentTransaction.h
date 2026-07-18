// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/SceneEditing/SceneDocument.h"

namespace SagaEditor
{
class UndoRedoStack;

class SceneDocumentTransaction
{
public:
    SceneDocumentTransaction(SceneDocument& document, UndoRedoStack& journal);
    [[nodiscard]] bool RenameEntity(std::string entityId, std::string name);
    [[nodiscard]] bool SetTransform(std::string entityId, SceneTransformReadModel transform);

private:
    SceneDocument& document_;
    UndoRedoStack& journal_;
};
}
