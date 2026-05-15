/// @file Cancellation.h
/// @brief Cooperative cancellation primitives for compiler integrations.

#pragma once

#include <atomic>

namespace SDE
{

class CancellationToken
{
public:
    CancellationToken() = default;

    [[nodiscard]] bool IsCancellationRequested() const noexcept;

private:
    friend class CancellationSource;
    explicit CancellationToken(const std::atomic_bool* flag) noexcept;

    const std::atomic_bool* mFlag = nullptr;
};

class CancellationSource
{
public:
    CancellationSource() = default;

    void RequestCancellation() noexcept;
    [[nodiscard]] CancellationToken Token() const noexcept;

private:
    mutable std::atomic_bool mCancelled{false};
};

} // namespace SDE
