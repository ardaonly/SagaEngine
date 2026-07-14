#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class CollaborationSessionState : uint8_t {
    Idle, Connecting, Connected, Reconnecting, Disconnected, Error
};
} // namespace SagaEditor::Collaboration
