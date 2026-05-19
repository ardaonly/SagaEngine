/// @file PublishReadiness.hpp
/// @brief Shared publish readiness contract.

#pragma once

#include "SagaShared/Publish/PublishBlocker.hpp"
#include "SagaShared/Publish/PublishStatus.hpp"

#include <vector>

namespace SagaShared::Publish
{

/// Current publish readiness and blocker list.
struct PublishReadiness
{
    PublishStatus status = PublishStatus::Unknown;
    std::vector<PublishBlocker> blockers;

    [[nodiscard]] bool IsReady() const noexcept
    {
        return status == PublishStatus::Ready ||
               status == PublishStatus::ReadyWithWarnings;
    }
};

} // namespace SagaShared::Publish
