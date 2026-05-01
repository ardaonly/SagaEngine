/// @file DerivedDataCache.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Pipeline/DerivedDataCache.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct DerivedDataCache::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

bool DerivedDataCache::Has(const std::string& /*key*/) const noexcept
{
    return false;
}

bool DerivedDataCache::Get(const std::string& /*key*/, std::vector<uint8_t>& /*out*/) const
{
    return false;
}

void DerivedDataCache::Put(const std::string& /*key*/, const std::vector<uint8_t>& /*data*/)
{
    /* stub */
}

void DerivedDataCache::Evict(const std::string& /*key*/)
{
    /* stub */
}

void DerivedDataCache::Clear()
{
    /* stub */
}

size_t DerivedDataCache::SizeBytes() const noexcept
{
    return 0;
}

} // namespace SagaEditor
