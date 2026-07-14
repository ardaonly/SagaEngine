#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class AuditEventType : uint8_t {
    SceneModified, AssetImported, UserJoined, UserLeft,
    LockAcquired, LockReleased, SessionStarted, SessionEnded,
};
} // namespace SagaEditor::Collaboration
