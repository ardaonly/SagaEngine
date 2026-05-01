/// @file OperationLog.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Replay/OperationLog.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct OperationLog::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

OperationLog::OperationLog()
    : m_impl(std::make_unique<Impl>())
{}

OperationLog::~OperationLog() = default;

void OperationLog::Append(const OperationLogEntry& /*entry*/)
{
    /* stub */
}

std::vector<OperationLogEntry> OperationLog::GetAll() const
{
    return {};
}

void OperationLog::Clear()
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
