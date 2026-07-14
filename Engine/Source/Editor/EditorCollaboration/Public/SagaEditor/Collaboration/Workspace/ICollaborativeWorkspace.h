#pragma once
#include "SagaEditor/Collaboration/Workspace/WorkspaceDocument.h"
#include <functional>
#include <vector>
namespace SagaEditor::Collaboration {
class ICollaborativeWorkspace {
public:
    virtual ~ICollaborativeWorkspace() = default;
    virtual std::vector<WorkspaceDocument> GetDocuments()  const = 0;
    virtual WorkspaceDocument* FindDocument(const std::string& id)  = 0;
    virtual void MarkModified(const std::string& documentId)        = 0;
    virtual void Sync()                                             = 0;
    virtual void SetOnDocumentChanged(std::function<void(const WorkspaceDocument&)> cb) = 0;
};
} // namespace SagaEditor::Collaboration
