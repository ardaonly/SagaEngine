#pragma once
#include <string>
namespace SagaEditor::Collaboration {
struct EditConflict {
    std::string objectId;
    std::string propertyPath;
    std::string localValue;
    std::string remoteValue;
};
} // namespace SagaEditor::Collaboration
