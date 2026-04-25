#pragma once
#include "SagaEditor/Collaboration/Audit/AuditEvent.h"
#include <vector>
namespace SagaEditor::Collaboration {
struct AuditFilter {
    AuditEventType typeFilter = AuditEventType::SceneModified;
    bool           filterByType = false;
    uint64_t       fromTimestamp = 0;
    uint64_t       toTimestamp   = 0;
    static std::vector<AuditEvent> Apply(const std::vector<AuditEvent>& src,
                                         const AuditFilter& f);
};
} // namespace SagaEditor::Collaboration
