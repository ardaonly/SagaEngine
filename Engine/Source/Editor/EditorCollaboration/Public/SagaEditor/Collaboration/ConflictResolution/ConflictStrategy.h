#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class ConflictStrategy : uint8_t { LastWriteWins, ServerAuthoritative, ThreeWayMerge };
} // namespace SagaEditor::Collaboration
