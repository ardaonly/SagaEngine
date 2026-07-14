/// @file AuditLogger.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Audit/AuditLogger.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct AuditLogger::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

AuditLogger::AuditLogger()
    : m_impl(std::make_unique<Impl>())
{}

AuditLogger::~AuditLogger() = default;

void AuditLogger::Log(const AuditEvent& /*event*/)
{
    /* stub */
}

const std::vector<AuditEvent>& AuditLogger::Events() const noexcept
{
    static std::vector<AuditEvent> s_default; return s_default;
}

void AuditLogger::Clear()
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
