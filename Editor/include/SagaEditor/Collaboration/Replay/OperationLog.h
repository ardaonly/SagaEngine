#pragma once
#include "SagaEditor/Collaboration/Replay/IOperationLog.h"
#include <memory>
namespace SagaEditor::Collaboration {
class OperationLog final : public IOperationLog {
public:
    OperationLog();
    ~OperationLog() override;
    void Append(const OperationLogEntry& entry) override;
    std::vector<OperationLogEntry> GetAll() const override;
    void Clear() override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
