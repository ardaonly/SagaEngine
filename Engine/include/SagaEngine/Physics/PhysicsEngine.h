/// @file PhysicsEngine.h
/// @brief High-level physics engine that wraps PhysicsWorld for easy integration.

#pragma once

#include "SagaEngine/Physics/PhysicsWorld.h"

namespace SagaEngine::Physics
{

/// Top-level physics engine — owns a PhysicsWorld and provides a
/// simplified tick interface for the game loop.  Later this will
/// manage sub-stepping, deterministic replay, and multiple scenes.
class PhysicsEngine
{
public:
    PhysicsEngine() = default;
    ~PhysicsEngine() = default;

    /// Initialize with a config (gravity, solver iterations, etc.).
    void Initialize(const PhysicsWorldConfig& cfg = {});

    /// Step the simulation by dt.  Internally may sub-step.
    void Tick(float dt);

    /// Shutdown and release all bodies.
    void Shutdown();

    /// Direct access to the underlying world.
    [[nodiscard]] PhysicsWorld&       GetWorld()       noexcept { return m_world; }
    [[nodiscard]] const PhysicsWorld& GetWorld() const noexcept { return m_world; }

    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
    PhysicsWorld m_world;
    float        m_fixedStep    = 1.0f / 60.0f;
    float        m_accumulator  = 0.0f;
    bool         m_initialized  = false;
};

} // namespace SagaEngine::Physics
