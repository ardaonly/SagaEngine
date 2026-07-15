#pragma once
#include <string>
namespace SagaEditor::VisualBlocks {
class NodeCategoryBrowser;
class NodeLibraryImport {
public:
    bool ImportLibrary(const std::string& path, NodeCategoryBrowser& out);
};
} // namespace SagaEditor::VisualBlocks
