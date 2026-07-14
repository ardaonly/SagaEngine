#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class WorkspaceDocumentState : uint8_t { Clean, Modified, Conflicted, Syncing };
} // namespace SagaEditor::Collaboration
