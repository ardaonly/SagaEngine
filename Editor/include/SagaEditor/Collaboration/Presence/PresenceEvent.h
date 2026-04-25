#pragma once
#include "SagaEditor/Collaboration/Presence/PresenceInfo.h"
namespace SagaEditor::Collaboration {
enum class PresenceEventType : uint8_t { Updated, Disconnected };
struct PresenceEvent { PresenceEventType type; PresenceInfo info; };
} // namespace SagaEditor::Collaboration
