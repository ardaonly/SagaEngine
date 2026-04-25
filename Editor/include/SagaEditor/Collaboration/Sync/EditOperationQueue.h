#pragma once
#include "SagaEditor/Collaboration/Sync/EditOperation.h"
#include <memory>
#include <vector>
namespace SagaEditor::Collaboration {
class EditOperationQueue {
public:
    EditOperationQueue();
    ~EditOperationQueue();
    void Enqueue(EditOperation op);
    bool TryDequeue(EditOperation& out);
    size_t Size() const noexcept;
    void Clear();
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
