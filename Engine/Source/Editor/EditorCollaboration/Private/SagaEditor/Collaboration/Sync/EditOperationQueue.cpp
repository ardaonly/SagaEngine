/// @file EditOperationQueue.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Sync/EditOperationQueue.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct EditOperationQueue::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

EditOperationQueue::EditOperationQueue()
    : m_impl(std::make_unique<Impl>())
{}

EditOperationQueue::~EditOperationQueue() = default;

void EditOperationQueue::Enqueue(EditOperation /*op*/)
{
    /* stub */
}

bool EditOperationQueue::TryDequeue(EditOperation& /*out*/)
{
    return false;
}

size_t EditOperationQueue::Size() const noexcept
{
    return 0;
}

void EditOperationQueue::Clear()
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
