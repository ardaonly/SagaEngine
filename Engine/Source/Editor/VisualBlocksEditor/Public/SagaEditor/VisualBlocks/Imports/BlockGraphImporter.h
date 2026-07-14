#pragma once
#include <string>
namespace SagaEditor::VisualScripting {
class GraphDocument;
class BlockGraphImporter {
public:
    bool Import(const std::string& path, GraphDocument& out);
    bool CanImport(const std::string& path) const noexcept;
};
} // namespace SagaEditor::VisualScripting
