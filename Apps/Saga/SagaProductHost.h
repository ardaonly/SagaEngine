/// @file SagaProductHost.h
/// @brief Product host boundary for preparing Saga role targets.

#pragma once

#include "SagaSessionModel.h"

#include <string>

namespace SagaProduct
{

/// Prepares product role module targets without owning their internals.
class SagaProductHost
{
public:
    /// Build the external process boundary for the requested session.
    [[nodiscard]] SagaPreparedTarget PrepareTarget(
        const SagaSessionModel& session) const;

    /// Validate and build the external process boundary for the requested session.
    [[nodiscard]] SagaTargetPreparationResult PrepareTargetWithDiagnostics(
        const SagaSessionModel& session) const;
};

} // namespace SagaProduct
