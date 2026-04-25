/// @file Component.cpp
/// @brief ECS component registry utilities.

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::ECS
{

// ComponentArray and TypedComponentArray are currently header-only
// (template code).  This file anchors future non-template helpers:
//
//   void LogComponentStats(const ComponentArray& arr);
//   void CompactAll(std::vector<std::unique_ptr<ComponentArray>>& arrays);

} // namespace SagaEngine::ECS
