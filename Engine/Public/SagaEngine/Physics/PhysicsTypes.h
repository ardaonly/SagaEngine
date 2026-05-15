/// @file PhysicsTypes.h
/// @brief Core data types for the physics simulation.
///
/// Layer  : SagaEngine / Physics
/// Purpose: Defines rigid body descriptors, collision shapes, and contact
///          structures used by PhysicsWorld.  All types are CPU-side data
///          with no GPU or renderer dependency.
///
/// Design rules:
///   - Shapes are value types — small, copyable, no heap allocs.
///   - RigidBody carries both physical properties (mass, restitution)
///     and the current simulation state (position, velocity).
///   - Contact manifold is minimal: one point per pair per frame.
///   - No inheritance — collision dispatch uses shape type enum + switch.

#pragma once

#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Math/Mat4.h"

#include <cstdint>
#include <limits>

namespace SagaEngine::Physics
{

// ─── Body type ───────────────────────────────────────────────────────────────

/// How the physics solver treats this body.
enum class BodyType : std::uint8_t
{
    Static    = 0,   ///< Never moves. Infinite mass. Used for terrain/walls.
    Dynamic   = 1,   ///< Fully simulated — affected by gravity and collisions.
    Kinematic = 2,   ///< Moved by gameplay code; pushes dynamic bodies but
                     ///  is not affected by forces or collisions itself.
};

// ─── Collision shape ─────────────────────────────────────────────────────────

/// Shape type discriminator.
enum class ShapeType : std::uint8_t
{
    Sphere  = 0,
    Box     = 1,   ///< Axis-aligned bounding box (AABB) in local space.
    Capsule = 2,   ///< Vertical capsule (Y-axis aligned).
};

/// A collision shape attached to a rigid body.  Always defined in the
/// body's local space — the world transform is applied at query time.
struct CollisionShape
{
    ShapeType type = ShapeType::Sphere;

    /// Sphere: radius.  Box: half-extents.  Capsule: (radius, halfHeight, 0).
    Math::Vec3 params{ 0.5f, 0.5f, 0.5f };

    /// Local-space offset from the body's origin.
    Math::Vec3 offset{ 0.0f, 0.0f, 0.0f };

    // ── Named constructors ───────────────────────────────────────────────────

    [[nodiscard]] static CollisionShape Sphere(float radius)
    {
        CollisionShape s;
        s.type   = ShapeType::Sphere;
        s.params = { radius, 0.0f, 0.0f };
        return s;
    }

    [[nodiscard]] static CollisionShape Box(float hx, float hy, float hz)
    {
        CollisionShape s;
        s.type   = ShapeType::Box;
        s.params = { hx, hy, hz };
        return s;
    }

    [[nodiscard]] static CollisionShape Capsule(float radius, float halfHeight)
    {
        CollisionShape s;
        s.type   = ShapeType::Capsule;
        s.params = { radius, halfHeight, 0.0f };
        return s;
    }
};

// ─── Body handle ─────────────────────────────────────────────────────────────

/// Opaque handle to a body in the PhysicsWorld. Returned from AddBody.
enum class BodyId : std::uint32_t
{
    kInvalid = 0,
};

// ─── Rigid body descriptor ───────────────────────────────────────────────────

/// Configuration for a new rigid body.  Passed to PhysicsWorld::AddBody().
struct RigidBodyDesc
{
    BodyType       type        = BodyType::Dynamic;
    CollisionShape shape       = CollisionShape::Sphere(0.5f);
    Math::Vec3     position    { 0.0f, 0.0f, 0.0f };
    Math::Quat     rotation    = Math::Quat::Identity();
    Math::Vec3     velocity    { 0.0f, 0.0f, 0.0f };
    float          mass        = 1.0f;       ///< Ignored for Static/Kinematic.
    float          restitution = 0.3f;       ///< Bounciness [0, 1].
    float          friction    = 0.5f;       ///< Surface friction [0, 1].
    float          linearDamping = 0.01f;    ///< Per-second velocity decay.
    bool           isTrigger   = false;      ///< Triggers generate events but no response.
    std::uint32_t  userData    = 0;          ///< Gameplay tag — the world doesn't interpret this.
};

// ─── Runtime body state ──────────────────────────────────────────────────────

/// Internal simulation state for one rigid body.  Stored in a flat array
/// inside PhysicsWorld for cache-friendly iteration.
struct RigidBody
{
    BodyId         id         = BodyId::kInvalid;
    BodyType       type       = BodyType::Static;
    CollisionShape shape{};
    Math::Vec3     position   { 0.0f, 0.0f, 0.0f };
    Math::Quat     rotation   = Math::Quat::Identity();
    Math::Vec3     velocity   { 0.0f, 0.0f, 0.0f };
    Math::Vec3     force      { 0.0f, 0.0f, 0.0f };  ///< Accumulated force for this step.
    float          invMass    = 1.0f;       ///< 0 for static/kinematic bodies.
    float          restitution= 0.3f;
    float          friction   = 0.5f;
    float          linearDamping = 0.01f;
    bool           isTrigger  = false;
    bool           active     = true;       ///< False = removed / sleeping.
    std::uint32_t  userData   = 0;
};

// ─── Contact ─────────────────────────────────────────────────────────────────

/// One contact point between two bodies in a single frame.
struct Contact
{
    BodyId     bodyA = BodyId::kInvalid;
    BodyId     bodyB = BodyId::kInvalid;
    Math::Vec3 point  { 0.0f, 0.0f, 0.0f };   ///< World-space contact point.
    Math::Vec3 normal { 0.0f, 1.0f, 0.0f };   ///< Points from A → B.
    float      depth  = 0.0f;                  ///< Penetration depth (positive = overlapping).
};

// ─── Raycast result ──────────────────────────────────────────────────────────

struct RaycastHit
{
    BodyId     body     = BodyId::kInvalid;
    Math::Vec3 point    { 0.0f, 0.0f, 0.0f };
    Math::Vec3 normal   { 0.0f, 1.0f, 0.0f };
    float      distance = std::numeric_limits<float>::max();

    [[nodiscard]] bool Hit() const noexcept { return body != BodyId::kInvalid; }
};

// ─── Limits ──────────────────────────────────────────────────────────────────

inline constexpr std::uint32_t kMaxBodies   = 4096;
inline constexpr float         kDefaultGravity = -9.81f;

} // namespace SagaEngine::Physics
