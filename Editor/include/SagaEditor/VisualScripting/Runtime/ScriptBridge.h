#pragma once
#include <string>
namespace SagaEditor::VisualScripting {
class ScriptBridge {
public:
    bool   CallManaged(const std::string& typeName,
                        const std::string& methodName,
                        const std::string& argsJson,
                        std::string&       resultJson);
};
} // namespace SagaEditor::VisualScripting
