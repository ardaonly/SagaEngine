#pragma once
#include "SagaEditor/Collaboration/ConflictResolution/ConflictStrategy.h"
#include "SagaEditor/Collaboration/ConflictResolution/EditConflict.h"
#include <string>
namespace SagaEditor::Collaboration {
class ConflictResolutionPipeline {
public:
    explicit ConflictResolutionPipeline(ConflictStrategy strategy = ConflictStrategy::LastWriteWins);
    std::string Resolve(const EditConflict& conflict);
    void SetStrategy(ConflictStrategy strategy);
private:
    ConflictStrategy m_strategy;
};
} // namespace SagaEditor::Collaboration
