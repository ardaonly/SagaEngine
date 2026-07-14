#pragma once
#include "SagaEditor/Collaboration/Locks/LockToken.h"
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class LockEventType : uint8_t { Acquired, Released, Expired, Denied };
struct LockEvent { LockEventType type; LockToken token; };
} // namespace SagaEditor::Collaboration
