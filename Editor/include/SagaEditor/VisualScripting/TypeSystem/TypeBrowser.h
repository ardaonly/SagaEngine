#pragma once
#include <string>
#include <vector>
namespace SagaEditor::VisualScripting {
struct TypeEntry { std::string fullName; std::string displayName; std::string category; };
class TypeBrowser {
public:
    void RegisterType(TypeEntry entry);
    std::vector<TypeEntry> Search(const std::string& query) const;
    std::vector<TypeEntry> GetByCategory(const std::string& cat) const;
private:
    std::vector<TypeEntry> m_types;
};
} // namespace SagaEditor::VisualScripting
