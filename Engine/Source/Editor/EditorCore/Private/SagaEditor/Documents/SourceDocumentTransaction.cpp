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
    CSharpTextAction(CSharpSourceDocument& document, std::string text,
        std::string label = "Edit C# Source", bool alreadyExecuted = false)
        : document_(document), text_(std::move(text)), label_(std::move(label)),
          executed_(alreadyExecuted), succeeded_(alreadyExecuted) {}
    void Execute() override
    {
        if (executed_) (void)document_.Redo();
        else { succeeded_ = document_.ReplaceText(text_); executed_ = true; }
    }
    void Undo() override { if (succeeded_) (void)document_.Undo(); }
    std::string GetLabel() const override { return label_; }
private:
    CSharpSourceDocument& document_;
    std::string text_;
    std::string label_;
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

bool CSharpDocumentTransaction::ReplaceTextProvisional(
    std::string text,
    std::function<bool(const CSharpSourceDocument&)> validateCandidate,
    std::string label)
{
    if (text.find('\0') != std::string::npos || text == document_.Text()) return false;
    const auto candidate = text;
    if (!document_.ReplaceTextProvisional(std::move(text), validateCandidate)) return false;
    journal_.PushExecuted(std::make_unique<CSharpTextAction>(
        document_, candidate, std::move(label), true));
    return true;
}
}
