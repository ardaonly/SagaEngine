/// @file Vec3.cpp
/// @brief Vec3 non-inline method implementations.

#include "SagaEngine/Math/Vec3.h"

#include <cstdio>

namespace SagaEngine::Math
{

// ─── Diagnostics ──────────────────────────────────────────────────────────────

std::string Vec3::ToString() const
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "(%.4f, %.4f, %.4f)", x, y, z);
    return buf;
}

} // namespace SagaEngine::Math
