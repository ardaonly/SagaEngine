#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class PermissionDeniedReason : uint8_t { InsufficientRole, ObjectLocked, SessionReadOnly };
} // namespace SagaEditor::Collaboration
