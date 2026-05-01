/// @file AssetCooking.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Pipeline/AssetCooking.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct AssetCooking::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

AssetCooking::AssetCooking()
    : m_impl(std::make_unique<Impl>())
{}

AssetCooking::~AssetCooking() = default;

CookResult AssetCooking::Cook(const CookJob& /*job*/)
{
    return {};
}

void AssetCooking::CookAsync(const CookJob& /*job*/, std::function<void(CookResult)> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor
