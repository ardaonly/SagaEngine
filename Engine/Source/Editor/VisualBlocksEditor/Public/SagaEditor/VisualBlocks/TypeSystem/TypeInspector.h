#pragma once
#include <string>
#include <vector>
namespace SagaEditor::VisualScripting {
struct TypeMember { std::string name; std::string typeName; bool isReadOnly; };
class TypeInspector {
public:
    std::vector<TypeMember> GetMembers(const std::string& typeName) const;
    bool                    IsAssignableTo(const std::string& from, const std::string& to) const;
};
} // namespace SagaEditor::VisualScripting
