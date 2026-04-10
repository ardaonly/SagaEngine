/// @file Transform.cpp
/// @brief Transform non-inline method implementations.

#include "SagaEngine/Math/Transform.h"

#include <cstdio>

namespace SagaEngine::Math
{

// ─── Diagnostics ──────────────────────────────────────────────────────────────

std::string Transform::ToString() const
{
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "Transform{ pos=%s rot=%s scale=%s }",
                  position.ToString().c_str(),
                  rotation.ToString().c_str(),
                  scale.ToString().c_str());
    return buf;
}

} // namespace SagaEngine::Math
