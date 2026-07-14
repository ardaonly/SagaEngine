#pragma once
#include <string>
namespace SagaEditor::VisualScripting {
class GraphDocument;
class VisualScriptingGraphImporter {
public:
    bool Import(const std::string& sourcePath, GraphDocument& out);
    bool CanImport(const std::string& path) const noexcept;
};
} // namespace SagaEditor::VisualScripting
