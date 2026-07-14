#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class AuthorityEventType : uint8_t { Elected, Revoked, HandoffStarted, HandoffCompleted };
struct AuthorityEvent { AuthorityEventType type; uint64_t userId; };
} // namespace SagaEditor::Collaboration
