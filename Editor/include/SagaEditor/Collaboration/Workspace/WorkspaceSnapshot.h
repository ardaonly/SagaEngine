#pragma once
#include "SagaEditor/Collaboration/Workspace/WorkspaceDocument.h"
#include <vector>
namespace SagaEditor::Collaboration {
struct WorkspaceSnapshot {
    uint64_t                       timestamp = 0;
    std::vector<WorkspaceDocument> documents;
};
} // namespace SagaEditor::Collaboration
