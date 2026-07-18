// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/SourceDocumentTransaction.h"

#include "SagaEditor/Commands/UndoRedoStack.h"

namespace SagaEditor
{
namespace
{
class CSharpTextAction final : public IEditorAction
{
public:
    CSharpTextAction(CSharpSourceDocument& document, std::string text)
        : document_(document), text_(std::move(text)) {}
    void Execute() override
    {
        if (executed_) (void)document_.Redo();
        else { succeeded_ = document_.ReplaceText(text_); executed_ = true; }
    }
    void Undo() override { if (succeeded_) (void)document_.Undo(); }
    std::string GetLabel() const override { return "Edit C# Source"; }
private:
    CSharpSourceDocument& document_;
    std::string text_;
    bool executed_ = false;
    bool succeeded_ = false;
};
}

CSharpDocumentTransaction::CSharpDocumentTransaction(CSharpSourceDocument& document,
                                                     UndoRedoStack& journal)
    : document_(document), journal_(journal) {}

bool CSharpDocumentTransaction::ReplaceText(std::string text)
{
    if (text.find('\0') != std::string::npos || text == document_.Text()) return false;
    journal_.Push(std::make_unique<CSharpTextAction>(document_, std::move(text)));
    return true;
}
}
