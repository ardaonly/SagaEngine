#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class WorkspaceSyncPolicy : uint8_t { AutoSync, ManualSync, ReadOnly };
} // namespace SagaEditor::Collaboration
