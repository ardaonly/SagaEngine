#pragma once
#include <functional>
#include <string>
#include <vector>
namespace SagaEditor::VisualScripting {
struct NodeCategoryEntry { std::string category; std::string nodeType; std::string label; };
class NodeCategoryBrowser {
public:
    void RegisterNode(const std::string& category,
                      const std::string& nodeType,
                      const std::string& label);
    std::vector<NodeCategoryEntry> GetAll()               const;
    std::vector<NodeCategoryEntry> Search(const std::string& query) const;
private:
    std::vector<NodeCategoryEntry> m_entries;
};
} // namespace SagaEditor::VisualScripting
