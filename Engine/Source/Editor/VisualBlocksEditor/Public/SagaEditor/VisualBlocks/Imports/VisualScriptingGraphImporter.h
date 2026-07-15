#pragma once
#include <string>
namespace SagaEditor::VisualBlocks {
class GraphDocument;
class VisualScriptingGraphImporter {
public:
    bool Import(const std::string& sourcePath, GraphDocument& out);
    bool CanImport(const std::string& path) const noexcept;
};
} // namespace SagaEditor::VisualBlocks
