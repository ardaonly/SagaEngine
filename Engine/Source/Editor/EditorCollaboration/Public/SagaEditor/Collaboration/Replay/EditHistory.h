#pragma once
#include "SagaEditor/Collaboration/Replay/OperationLogEntry.h"
#include <vector>
namespace SagaEditor::Collaboration {
class EditHistory {
public:
    void Record(const OperationLogEntry& entry);
    bool CanUndo() const noexcept;
    bool CanRedo() const noexcept;
    OperationLogEntry Undo();
    OperationLogEntry Redo();
    void Clear();
private:
    std::vector<OperationLogEntry> m_log;
    size_t m_cursor = 0;
};
} // namespace SagaEditor::Collaboration
