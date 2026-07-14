#pragma once
#include "SagaEditor/Collaboration/Audit/AuditEvent.h"
namespace SagaEditor::Collaboration {
class IAuditLogger {
public:
    virtual ~IAuditLogger() = default;
    virtual void Log(const AuditEvent& event) = 0;
};
} // namespace SagaEditor::Collaboration
