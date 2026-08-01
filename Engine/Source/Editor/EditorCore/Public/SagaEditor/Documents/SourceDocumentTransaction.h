// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/SourceDocuments.h"

#include <functional>

namespace SagaEditor
{
class UndoRedoStack;

class CSharpDocumentTransaction
{
public:
    CSharpDocumentTransaction(CSharpSourceDocument& document, UndoRedoStack& journal);
    [[nodiscard]] bool ReplaceText(std::string text);
    [[nodiscard]] bool ReplaceTextProvisional(std::string text,
        std::function<bool(const CSharpSourceDocument&)> validateCandidate,
        std::string label = "Edit C# Source");
private:
    CSharpSourceDocument& document_;
    UndoRedoStack& journal_;
};
}
