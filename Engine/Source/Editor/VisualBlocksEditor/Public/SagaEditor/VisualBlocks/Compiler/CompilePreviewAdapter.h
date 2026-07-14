#pragma once
#include <string>
namespace SagaEditor::VisualScripting {
class GraphDocument;
class CompilePreviewAdapter {
public:
    std::string GeneratePreview(const GraphDocument& doc) const;
};
} // namespace SagaEditor::VisualScripting
