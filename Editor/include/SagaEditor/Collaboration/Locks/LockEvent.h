#pragma once
#include "SagaEditor/Collaboration/Locks/LockToken.h"
namespace SagaEditor::Collaboration {
enum class LockEventType : uint8_t { Acquired, Released, Expired, Denied };
struct LockEvent { LockEventType type; LockToken token; };
} // namespace SagaEditor::Collaboration
