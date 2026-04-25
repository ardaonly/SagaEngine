#pragma once
#include "SagaEditor/Collaboration/Audit/AuditEvent.h"
#include <string>
#include <vector>
namespace SagaEditor::Collaboration {
class AuditExporter {
public:
    static std::string ToJson(const std::vector<AuditEvent>& events);
    static bool        ToFile(const std::vector<AuditEvent>& events,
                               const std::string& path);
};
} // namespace SagaEditor::Collaboration
