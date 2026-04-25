#pragma once
#include "SagaEditor/Collaboration/Workspace/WorkspaceDocumentState.h"
#include <string>
namespace SagaEditor::Collaboration {
struct WorkspaceDocument {
    std::string            documentId;
    std::string            path;
    WorkspaceDocumentState state = WorkspaceDocumentState::Clean;
    uint64_t               lastModifiedBy = 0;
};
} // namespace SagaEditor::Collaboration
