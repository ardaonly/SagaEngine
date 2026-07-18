// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/SourceDocuments.h"

namespace SagaEditor
{
class UndoRedoStack;

class CSharpDocumentTransaction
{
public:
    CSharpDocumentTransaction(CSharpSourceDocument& document, UndoRedoStack& journal);
    [[nodiscard]] bool ReplaceText(std::string text);
private:
    CSharpSourceDocument& document_;
    UndoRedoStack& journal_;
};
}
