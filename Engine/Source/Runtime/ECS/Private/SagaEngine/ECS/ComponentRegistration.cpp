/// @file ComponentRegistration.cpp
/// @brief Central registration for all standard ECS components.
///
/// Layer  : SagaEngine / ECS
/// Purpose: Every component type used in WorldState must be registered with an
///          explicit, hard-coded uint32_t before any WorldState is created.
///          This file ensures cross-binary determinism: client, server, and
///          sandbox all agree on the same type IDs.
///
/// Thread safety: Register() is mutex-protected for safe concurrent startup.
///
/// IMPORTANT: Never change an existing component ID.  Only add new entries at
///            the end.  Removing a component requires a migration plan for
///            saved WorldState snapshots.

#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/ECS/Components/TransformComponent.h"

// Register standard engine components at startup via static initialization.
namespace {

struct StandardComponentRegistrar
{
    StandardComponentRegistrar()
    {
        using namespace SagaEngine::ECS;

        // Core spatial component — required for cell splitting,
        // visibility graph queries, and client interpolation.
        SAGA_REGISTER_COMPONENT(TransformComponent, 1);
    }
} g_standardComponentRegistrar;

} // anonymous namespace
