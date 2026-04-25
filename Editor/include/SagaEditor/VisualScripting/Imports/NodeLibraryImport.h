#pragma once
#include <string>
namespace SagaEditor::VisualScripting {
class NodeCategoryBrowser;
class NodeLibraryImport {
public:
    bool ImportLibrary(const std::string& path, NodeCategoryBrowser& out);
};
} // namespace SagaEditor::VisualScripting
