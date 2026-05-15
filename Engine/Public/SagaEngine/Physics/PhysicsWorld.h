/// @file PhysicsWorld.h
/// @brief The simulation container — owns bodies, steps the sim, resolves collisions.
///
/// Layer  : SagaEngine / Physics
/// Purpose: PhysicsWorld is the public API for the physics subsystem.  It
///          manages a flat array of rigid bodies, runs broadphase + narrowphase
///          each step, applies gravity and impulses, and exposes raycasting.
///
/// Design rules:
///   - Single-threaded step.  The world is ticked once per gameplay frame.
///   - No allocations during Step() beyond the pre-reserved contact buffer.
///   - Bodies are stored in a flat vector for cache-friendly iteration;
///     removed bodies are marked inactive and reused.
///   - The broadphase is a simple O(n²) sweep suitable for < 4096 bodies.
///     When body counts grow, a spatial hash can be plugged in without
///     changing the public API.

#pragma once

#include "SagaEngine/Physics/PhysicsTypes.h"

#include <cstdint>
#include <vector>

namespace SagaEngine::Physics
{

/// Configuration for the physics simulation.
struct PhysicsWorldConfig
{
    float gravity         = kDefaultGravity;   ///< Y-axis gravity (m/s²).
    int   solverIterations = 4;                ///< Position correction iterations.
    float maxPenetration   = 0.01f;            ///< Allowed overlap before correction.
    float baumgarte       = 0.2f;             ///< Position correction strength.
};

class PhysicsWorld
{
public:
    explicit PhysicsWorld(const PhysicsWorldConfig& cfg = {});
    ~PhysicsWorld() = default;

    // Non-copyable, moveable.
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&) noexcept = default;
    PhysicsWorld& operator=(PhysicsWorld&&) noexcept = default;

    // ── Body management ──────────────────────────────────────────────────────

    /// Add a body to the world.  Returns its handle.
    [[nodiscard]] BodyId AddBody(const RigidBodyDesc& desc);

    /// Remove a body (mark inactive, slot reused later).
    void RemoveBody(BodyId id);

    /// Get mutable body state.  Returns nullptr if id is invalid/removed.
    [[nodiscard]] RigidBody*       GetBody(BodyId id) noexcept;
    [[nodiscard]] const RigidBody* GetBody(BodyId id) const noexcept;

    /// Set the position of a kinematic body directly (teleport).
    void SetKinematicTarget(BodyId id, const Math::Vec3& position);

    /// Apply a force (in Newtons) to a dynamic body for this step.
    void ApplyForce(BodyId id, const Math::Vec3& force);

    /// Apply an instantaneous impulse (velocity change = impulse * invMass).
    void ApplyImpulse(BodyId id, const Math::Vec3& impulse);

    // ── Simulation ───────────────────────────────────────────────────────────

    /// Advance the simulation by dt seconds.  Typical call: Step(1/60).
    void Step(float dt);

    // ── Queries ──────────────────────────────────────────────────────────────

    /// Cast a ray and return the closest hit.
    [[nodiscard]] RaycastHit Raycast(const Math::Vec3& origin,
                                      const Math::Vec3& direction,
                                      float maxDistance = 1000.0f) const;

    // ── Diagnostics ──────────────────────────────────────────────────────────

    [[nodiscard]] std::uint32_t BodyCount()    const noexcept { return m_activeCount; }
    [[nodiscard]] std::uint32_t ContactCount() const noexcept { return static_cast<std::uint32_t>(m_contacts.size()); }
    [[nodiscard]] float         LastStepMs()   const noexcept { return m_lastStepMs; }

    /// Access the contact list from the last Step() for event processing.
    [[nodiscard]] const std::vector<Contact>& GetContacts() const noexcept { return m_contacts; }

private:
    void IntegrateVelocities(float dt);
    void Broadphase();
    void Narrowphase();
    void SolveContacts(float dt);
    void IntegratePositions(float dt);

    PhysicsWorldConfig       m_config;
    std::vector<RigidBody>   m_bodies;
    std::vector<Contact>     m_contacts;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> m_pairs;  ///< Broadphase output.
    std::uint32_t            m_nextId      = 1;
    std::uint32_t            m_activeCount = 0;
    float                    m_lastStepMs  = 0.0f;
};

} // namespace SagaEngine::Physics
