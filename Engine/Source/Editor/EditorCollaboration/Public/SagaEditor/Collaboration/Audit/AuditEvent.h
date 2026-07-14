#pragma once
#include "SagaEditor/Collaboration/Audit/AuditEventType.h"
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
struct AuditEvent {
    AuditEventType type    = AuditEventType::SceneModified;
    uint64_t       userId  = 0;
    uint64_t       timestamp = 0;  ///< Unix ms
    std::string    detail;
};
} // namespace SagaEditor::Collaboration
