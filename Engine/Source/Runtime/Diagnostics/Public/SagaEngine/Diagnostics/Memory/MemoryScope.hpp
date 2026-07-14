/// @file MemoryScope.hpp
/// @brief Provides RAII recording for explicit diagnostics memory scopes.

#pragma once

#include <SagaEngine/Diagnostics/Memory/MemoryTracker.hpp>

#include <cstdint>
#include <string>

namespace SagaEngine::Diagnostics
{

/// Move-only helper that pairs an explicit allocation with a deterministic free.
class MemoryScope
{
public:
    MemoryScope(MemoryTracker& tracker,
                std::string systemName,
                std::uint64_t bytes);
    ~MemoryScope();

    MemoryScope(const MemoryScope&) = delete;
    MemoryScope& operator=(const MemoryScope&) = delete;

    MemoryScope(MemoryScope&& other) noexcept;
    MemoryScope& operator=(MemoryScope&& other) noexcept;

    /// Release the explicit allocation before destruction.
    bool Release() noexcept;

private:
    MemoryTracker* tracker_ = nullptr;       ///< Non-owning tracker pointer.
    std::string systemName_;                 ///< System to free on destruction.
    std::uint64_t bytes_ = 0;                ///< Bytes to free on destruction.
    bool active_ = false;                    ///< True while the scope owns a record.
};

} // namespace SagaEngine::Diagnostics
