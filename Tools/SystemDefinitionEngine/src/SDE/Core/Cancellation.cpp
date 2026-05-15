/// @file Cancellation.cpp
/// @brief Cooperative cancellation primitives.

#include "SDE/Core/Cancellation.h"

namespace SDE
{

CancellationToken::CancellationToken(const std::atomic_bool* flag) noexcept
    : mFlag(flag)
{
}

bool CancellationToken::IsCancellationRequested() const noexcept
{
    return mFlag != nullptr && mFlag->load(std::memory_order_relaxed);
}

void CancellationSource::RequestCancellation() noexcept
{
    mCancelled.store(true, std::memory_order_relaxed);
}

CancellationToken CancellationSource::Token() const noexcept
{
    return CancellationToken(&mCancelled);
}

} // namespace SDE
