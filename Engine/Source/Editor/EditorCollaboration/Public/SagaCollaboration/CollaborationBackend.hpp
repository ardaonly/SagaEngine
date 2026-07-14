/// @file CollaborationBackend.hpp
/// @brief Backend adapter boundary for collaboration persistence and transport.

#pragma once

#include "SagaShared/Collaboration/CollaborationDiagnostic.hpp"

#include <vector>

namespace SagaCollaboration
{

/// Optional backend adapter consumed by collaboration services.
class CollaborationBackend
{
public:
    virtual ~CollaborationBackend() = default;

    /// Return backend availability diagnostics without exposing backend internals.
    [[nodiscard]] virtual std::vector<SagaShared::Collaboration::CollaborationDiagnostic>
    Diagnostics() const = 0;
};

} // namespace SagaCollaboration
