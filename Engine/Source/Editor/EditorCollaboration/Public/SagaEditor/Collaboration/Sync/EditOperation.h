#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
enum class EditOpKind : uint8_t { Set, Insert, Delete, Move };
struct EditOperation {
    uint64_t    sequenceId = 0;
    uint64_t    userId     = 0;
    EditOpKind  kind       = EditOpKind::Set;
    std::string path;
    std::string value;
};
} // namespace SagaEditor::Collaboration
