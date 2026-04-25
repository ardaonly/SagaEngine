#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
struct OperationLogEntry {
    uint64_t    sequenceId = 0;
    uint64_t    userId     = 0;
    uint64_t    timestamp  = 0; ///< Unix ms
    std::string opType;
    std::string payload;   ///< JSON-serialised operation data
};
} // namespace SagaEditor::Collaboration
