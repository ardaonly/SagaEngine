#pragma once
#include "SagaEditor/Collaboration/Workspace/ICollaborativeWorkspace.h"
#include <memory>
namespace SagaEditor::Collaboration {
class CollaborativeWorkspace final : public ICollaborativeWorkspace {
public:
    CollaborativeWorkspace();
    ~CollaborativeWorkspace() override;
    std::vector<WorkspaceDocument> GetDocuments()  const override;
    WorkspaceDocument* FindDocument(const std::string& id) override;
    void MarkModified(const std::string& documentId)       override;
    void Sync()                                            override;
    void SetOnDocumentChanged(std::function<void(const WorkspaceDocument&)> cb) override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
