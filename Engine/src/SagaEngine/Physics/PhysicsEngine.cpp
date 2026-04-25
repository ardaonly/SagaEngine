/// @file SagaPhysicsEngine.cpp
/// @brief PhysicsEngine implementation — fixed-timestep wrapper around PhysicsWorld.

#include "SagaEngine/Physics/PhysicsEngine.h"

namespace SagaEngine::Physics
{

void PhysicsEngine::Initialize(const PhysicsWorldConfig& cfg)
{
    m_world       = PhysicsWorld(cfg);
    m_fixedStep   = 1.0f / 60.0f;
    m_accumulator = 0.0f;
    m_initialized = true;
}

void PhysicsEngine::Tick(float dt)
{
    if (!m_initialized) return;

    // Fixed-timestep accumulator — ensures deterministic stepping
    // regardless of frame rate.  Cap accumulated time to prevent
    // spiral-of-death when frames are very long.
    constexpr float kMaxAccumulator = 0.25f;
    m_accumulator += dt;
    if (m_accumulator > kMaxAccumulator)
        m_accumulator = kMaxAccumulator;

    while (m_accumulator >= m_fixedStep)
    {
        m_world.Step(m_fixedStep);
        m_accumulator -= m_fixedStep;
    }
}

void PhysicsEngine::Shutdown()
{
    m_world       = PhysicsWorld{};
    m_accumulator = 0.0f;
    m_initialized = false;
}

} // namespace SagaEngine::Physics
