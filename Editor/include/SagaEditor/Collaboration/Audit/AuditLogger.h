#pragma once
#include "SagaEditor/Collaboration/Audit/IAuditLogger.h"
#include <memory>
#include <vector>
namespace SagaEditor::Collaboration {
class AuditLogger final : public IAuditLogger {
public:
    AuditLogger();
    ~AuditLogger() override;
    void Log(const AuditEvent& event) override;
    const std::vector<AuditEvent>& Events() const noexcept;
    void Clear();
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
