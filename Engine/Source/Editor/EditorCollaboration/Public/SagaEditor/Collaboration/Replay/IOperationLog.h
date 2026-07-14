#pragma once
#include "SagaEditor/Collaboration/Replay/OperationLogEntry.h"
#include <vector>
namespace SagaEditor::Collaboration {
class IOperationLog {
public:
    virtual ~IOperationLog() = default;
    virtual void Append(const OperationLogEntry& entry)     = 0;
    virtual std::vector<OperationLogEntry> GetAll() const   = 0;
    virtual void Clear()                                    = 0;
};
} // namespace SagaEditor::Collaboration
