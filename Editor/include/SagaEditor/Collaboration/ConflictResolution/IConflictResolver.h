#pragma once
#include "SagaEditor/Collaboration/ConflictResolution/EditConflict.h"
#include <string>
namespace SagaEditor::Collaboration {
class IConflictResolver {
public:
    virtual ~IConflictResolver() = default;
    virtual std::string Resolve(const EditConflict& conflict) = 0;
};
} // namespace SagaEditor::Collaboration
