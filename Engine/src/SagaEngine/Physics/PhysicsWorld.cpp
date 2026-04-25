/// @file PhysicsWorld.cpp
/// @brief Rigid body simulation — integration, collision, response.

#include "SagaEngine/Physics/PhysicsWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace SagaEngine::Physics
{

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace
{

float Dot(const Math::Vec3& a, const Math::Vec3& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float LengthSq(const Math::Vec3& v) noexcept
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float Length(const Math::Vec3& v) noexcept
{
    return std::sqrt(LengthSq(v));
}

Math::Vec3 Normalize(const Math::Vec3& v) noexcept
{
    const float len = Length(v);
    if (len < 1e-8f) return { 0.0f, 0.0f, 0.0f };
    return { v.x / len, v.y / len, v.z / len };
}

// Vec3 arithmetic operators are provided by Vec3.h member functions.
// Only free-function helpers not in Vec3.h are defined here (Dot, etc.).

/// AABB for broadphase — computed from body position + shape.
struct AABB
{
    Math::Vec3 min{};
    Math::Vec3 max{};
};

AABB ComputeAABB(const RigidBody& body) noexcept
{
    const auto& pos = body.position;
    const auto& p   = body.shape.params;
    const auto& off = body.shape.offset;

    Math::Vec3 centre = pos + off;
    Math::Vec3 half{};

    switch (body.shape.type)
    {
    case ShapeType::Sphere:
        half = { p.x, p.x, p.x };  // p.x = radius
        break;
    case ShapeType::Box:
        half = p;  // half-extents directly
        break;
    case ShapeType::Capsule:
        // p.x = radius, p.y = halfHeight
        half = { p.x, p.y + p.x, p.x };
        break;
    }

    return {
        { centre.x - half.x, centre.y - half.y, centre.z - half.z },
        { centre.x + half.x, centre.y + half.y, centre.z + half.z },
    };
}

bool AABBOverlap(const AABB& a, const AABB& b) noexcept
{
    if (a.max.x < b.min.x || b.max.x < a.min.x) return false;
    if (a.max.y < b.min.y || b.max.y < a.min.y) return false;
    if (a.max.z < b.min.z || b.max.z < a.min.z) return false;
    return true;
}

// ─── Narrowphase collision tests ─────────────────────────────────────────────

/// Sphere vs Sphere.
bool TestSphereSphere(const RigidBody& a, const RigidBody& b, Contact& out) noexcept
{
    Math::Vec3 posA = a.position + a.shape.offset;
    Math::Vec3 posB = b.position + b.shape.offset;
    float rA = a.shape.params.x;
    float rB = b.shape.params.x;

    Math::Vec3 diff = posB - posA;
    float distSq = LengthSq(diff);
    float sumR = rA + rB;

    if (distSq >= sumR * sumR) return false;

    float dist = std::sqrt(distSq);
    out.normal = (dist > 1e-8f) ? Normalize(diff) : Math::Vec3{ 0.0f, 1.0f, 0.0f };
    out.depth  = sumR - dist;
    out.point  = posA + out.normal * (rA - out.depth * 0.5f);
    return true;
}

/// Sphere vs Box (AABB).
bool TestSphereBox(const RigidBody& sphere, const RigidBody& box, Contact& out) noexcept
{
    Math::Vec3 sPos = sphere.position + sphere.shape.offset;
    Math::Vec3 bPos = box.position + box.shape.offset;
    float radius = sphere.shape.params.x;
    const auto& half = box.shape.params;

    // Closest point on AABB to sphere centre.
    Math::Vec3 local = sPos - bPos;
    Math::Vec3 clamped{
        std::clamp(local.x, -half.x, half.x),
        std::clamp(local.y, -half.y, half.y),
        std::clamp(local.z, -half.z, half.z),
    };

    Math::Vec3 closest = bPos + clamped;
    Math::Vec3 diff = sPos - closest;
    float distSq = LengthSq(diff);

    if (distSq >= radius * radius) return false;

    float dist = std::sqrt(distSq);
    // If sphere centre is inside box, push along the shortest axis.
    if (dist < 1e-8f)
    {
        // Find the axis with smallest penetration.
        float dx = half.x - std::abs(local.x);
        float dy = half.y - std::abs(local.y);
        float dz = half.z - std::abs(local.z);

        if (dx <= dy && dx <= dz)
            out.normal = { (local.x >= 0.0f ? 1.0f : -1.0f), 0.0f, 0.0f };
        else if (dy <= dz)
            out.normal = { 0.0f, (local.y >= 0.0f ? 1.0f : -1.0f), 0.0f };
        else
            out.normal = { 0.0f, 0.0f, (local.z >= 0.0f ? 1.0f : -1.0f) };

        out.depth = radius + std::min({ dx, dy, dz });
    }
    else
    {
        out.normal = Normalize(diff);
        out.depth  = radius - dist;
    }

    out.point = closest;
    return true;
}

/// Box vs Box (AABB vs AABB — separating axis test).
bool TestBoxBox(const RigidBody& a, const RigidBody& b, Contact& out) noexcept
{
    Math::Vec3 posA = a.position + a.shape.offset;
    Math::Vec3 posB = b.position + b.shape.offset;
    const auto& hA = a.shape.params;
    const auto& hB = b.shape.params;

    Math::Vec3 diff = posB - posA;

    float overlapX = (hA.x + hB.x) - std::abs(diff.x);
    if (overlapX <= 0.0f) return false;

    float overlapY = (hA.y + hB.y) - std::abs(diff.y);
    if (overlapY <= 0.0f) return false;

    float overlapZ = (hA.z + hB.z) - std::abs(diff.z);
    if (overlapZ <= 0.0f) return false;

    // Choose the axis with the smallest overlap.
    if (overlapX <= overlapY && overlapX <= overlapZ)
    {
        out.normal = { (diff.x >= 0.0f ? 1.0f : -1.0f), 0.0f, 0.0f };
        out.depth  = overlapX;
    }
    else if (overlapY <= overlapZ)
    {
        out.normal = { 0.0f, (diff.y >= 0.0f ? 1.0f : -1.0f), 0.0f };
        out.depth  = overlapY;
    }
    else
    {
        out.normal = { 0.0f, 0.0f, (diff.z >= 0.0f ? 1.0f : -1.0f) };
        out.depth  = overlapZ;
    }

    // Contact point: midpoint of the overlap region on the contact axis.
    out.point = posA + diff * 0.5f;
    return true;
}

/// Sphere vs Capsule (vertical capsule along Y).
bool TestSphereCapsule(const RigidBody& sphere, const RigidBody& capsule, Contact& out) noexcept
{
    Math::Vec3 sPos = sphere.position + sphere.shape.offset;
    Math::Vec3 cPos = capsule.position + capsule.shape.offset;
    float sR = sphere.shape.params.x;
    float cR = capsule.shape.params.x;
    float cH = capsule.shape.params.y;  // half-height of the line segment

    // Clamp sphere Y onto the capsule's line segment.
    float projY = std::clamp(sPos.y - cPos.y, -cH, cH);
    Math::Vec3 closestOnCapsule = { cPos.x, cPos.y + projY, cPos.z };

    Math::Vec3 diff = sPos - closestOnCapsule;
    float distSq = LengthSq(diff);
    float sumR = sR + cR;

    if (distSq >= sumR * sumR) return false;

    float dist = std::sqrt(distSq);
    out.normal = (dist > 1e-8f) ? Normalize(diff) : Math::Vec3{ 0.0f, 1.0f, 0.0f };
    out.depth  = sumR - dist;
    out.point  = closestOnCapsule + out.normal * cR;
    return true;
}

/// Dispatch narrowphase based on shape types.
bool TestCollision(const RigidBody& a, const RigidBody& b, Contact& out) noexcept
{
    const auto tA = a.shape.type;
    const auto tB = b.shape.type;

    // Sphere vs Sphere
    if (tA == ShapeType::Sphere && tB == ShapeType::Sphere)
        return TestSphereSphere(a, b, out);

    // Sphere vs Box (ordered)
    if (tA == ShapeType::Sphere && tB == ShapeType::Box)
        return TestSphereBox(a, b, out);
    if (tA == ShapeType::Box && tB == ShapeType::Sphere)
    {
        bool hit = TestSphereBox(b, a, out);
        if (hit) out.normal = out.normal * -1.0f;  // Flip: A→B
        return hit;
    }

    // Box vs Box
    if (tA == ShapeType::Box && tB == ShapeType::Box)
        return TestBoxBox(a, b, out);

    // Sphere vs Capsule
    if (tA == ShapeType::Sphere && tB == ShapeType::Capsule)
        return TestSphereCapsule(a, b, out);
    if (tA == ShapeType::Capsule && tB == ShapeType::Sphere)
    {
        bool hit = TestSphereCapsule(b, a, out);
        if (hit) out.normal = out.normal * -1.0f;
        return hit;
    }

    // TODO: Capsule vs Box, Capsule vs Capsule — approximate with sphere for now.
    // Fallback: treat both as spheres using max extent.
    return false;
}

// ─── Ray intersection helpers ────────────────────────────────────────────────

bool RaySphere(const Math::Vec3& origin, const Math::Vec3& dir,
               const Math::Vec3& centre, float radius,
               float& outDist) noexcept
{
    Math::Vec3 oc = origin - centre;
    float b = Dot(oc, dir);
    float c = Dot(oc, oc) - radius * radius;

    float disc = b * b - c;
    if (disc < 0.0f) return false;

    float sqrtDisc = std::sqrt(disc);
    float t = -b - sqrtDisc;
    if (t < 0.0f) t = -b + sqrtDisc;
    if (t < 0.0f) return false;

    outDist = t;
    return true;
}

bool RayAABB(const Math::Vec3& origin, const Math::Vec3& dir,
             const AABB& box, float& outDist) noexcept
{
    float tMin = 0.0f;
    float tMax = 1e30f;

    auto slab = [&](float o, float d, float bmin, float bmax) -> bool
    {
        if (std::abs(d) < 1e-8f)
            return (o >= bmin && o <= bmax);

        float t1 = (bmin - o) / d;
        float t2 = (bmax - o) / d;
        if (t1 > t2) std::swap(t1, t2);

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        return tMin <= tMax;
    };

    if (!slab(origin.x, dir.x, box.min.x, box.max.x)) return false;
    if (!slab(origin.y, dir.y, box.min.y, box.max.y)) return false;
    if (!slab(origin.z, dir.z, box.min.z, box.max.z)) return false;

    if (tMin < 0.0f) tMin = tMax;
    if (tMin < 0.0f) return false;

    outDist = tMin;
    return true;
}

} // anonymous namespace

// ─── Construction ────────────────────────────────────────────────────────────

PhysicsWorld::PhysicsWorld(const PhysicsWorldConfig& cfg)
    : m_config(cfg)
{
    m_bodies.reserve(256);
    m_contacts.reserve(128);
    m_pairs.reserve(256);
}

// ─── Body management ─────────────────────────────────────────────────────────

BodyId PhysicsWorld::AddBody(const RigidBodyDesc& desc)
{
    RigidBody body{};
    body.id          = static_cast<BodyId>(m_nextId++);
    body.type        = desc.type;
    body.shape       = desc.shape;
    body.position    = desc.position;
    body.rotation    = desc.rotation;
    body.velocity    = desc.velocity;
    body.restitution = desc.restitution;
    body.friction    = desc.friction;
    body.linearDamping = desc.linearDamping;
    body.isTrigger   = desc.isTrigger;
    body.userData    = desc.userData;
    body.active      = true;

    // Inverse mass: 0 for static/kinematic, 1/mass for dynamic.
    body.invMass = (desc.type == BodyType::Dynamic && desc.mass > 0.0f)
                 ? (1.0f / desc.mass)
                 : 0.0f;

    // Try to reuse an inactive slot.
    for (auto& slot : m_bodies)
    {
        if (!slot.active)
        {
            slot = body;
            ++m_activeCount;
            return body.id;
        }
    }

    m_bodies.push_back(body);
    ++m_activeCount;
    return body.id;
}

void PhysicsWorld::RemoveBody(BodyId id)
{
    for (auto& b : m_bodies)
    {
        if (b.id == id && b.active)
        {
            b.active = false;
            --m_activeCount;
            return;
        }
    }
}

RigidBody* PhysicsWorld::GetBody(BodyId id) noexcept
{
    for (auto& b : m_bodies)
        if (b.id == id && b.active)
            return &b;
    return nullptr;
}

const RigidBody* PhysicsWorld::GetBody(BodyId id) const noexcept
{
    for (const auto& b : m_bodies)
        if (b.id == id && b.active)
            return &b;
    return nullptr;
}

void PhysicsWorld::SetKinematicTarget(BodyId id, const Math::Vec3& position)
{
    if (auto* b = GetBody(id))
    {
        if (b->type == BodyType::Kinematic)
            b->position = position;
    }
}

void PhysicsWorld::ApplyForce(BodyId id, const Math::Vec3& force)
{
    if (auto* b = GetBody(id))
    {
        if (b->type == BodyType::Dynamic)
            b->force = b->force + force;
    }
}

void PhysicsWorld::ApplyImpulse(BodyId id, const Math::Vec3& impulse)
{
    if (auto* b = GetBody(id))
    {
        if (b->type == BodyType::Dynamic)
            b->velocity = b->velocity + impulse * b->invMass;
    }
}

// ─── Simulation step ─────────────────────────────────────────────────────────

void PhysicsWorld::Step(float dt)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    if (dt <= 0.0f) return;

    IntegrateVelocities(dt);
    Broadphase();
    Narrowphase();
    SolveContacts(dt);
    IntegratePositions(dt);

    // Clear accumulated forces.
    for (auto& b : m_bodies)
    {
        if (b.active && b.type == BodyType::Dynamic)
            b.force = { 0.0f, 0.0f, 0.0f };
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    m_lastStepMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

void PhysicsWorld::IntegrateVelocities(float dt)
{
    const Math::Vec3 gravity{ 0.0f, m_config.gravity, 0.0f };

    for (auto& b : m_bodies)
    {
        if (!b.active || b.type != BodyType::Dynamic) continue;

        // Apply gravity + accumulated forces.
        Math::Vec3 accel = gravity + b.force * b.invMass;
        b.velocity = b.velocity + accel * dt;

        // Linear damping.
        const float damping = std::max(0.0f, 1.0f - b.linearDamping * dt);
        b.velocity = b.velocity * damping;
    }
}

void PhysicsWorld::Broadphase()
{
    m_pairs.clear();

    const auto n = static_cast<std::uint32_t>(m_bodies.size());
    for (std::uint32_t i = 0; i < n; ++i)
    {
        if (!m_bodies[i].active) continue;

        AABB aabbI = ComputeAABB(m_bodies[i]);

        for (std::uint32_t j = i + 1; j < n; ++j)
        {
            if (!m_bodies[j].active) continue;

            // Skip static-static pairs — they can never collide.
            if (m_bodies[i].type == BodyType::Static &&
                m_bodies[j].type == BodyType::Static)
                continue;

            AABB aabbJ = ComputeAABB(m_bodies[j]);
            if (AABBOverlap(aabbI, aabbJ))
                m_pairs.emplace_back(i, j);
        }
    }
}

void PhysicsWorld::Narrowphase()
{
    m_contacts.clear();

    for (const auto& [i, j] : m_pairs)
    {
        Contact c{};
        if (TestCollision(m_bodies[i], m_bodies[j], c))
        {
            c.bodyA = m_bodies[i].id;
            c.bodyB = m_bodies[j].id;
            m_contacts.push_back(c);
        }
    }
}

void PhysicsWorld::SolveContacts(float dt)
{
    if (m_contacts.empty()) return;

    for (int iter = 0; iter < m_config.solverIterations; ++iter)
    {
        for (const auto& c : m_contacts)
        {
            RigidBody* a = GetBody(c.bodyA);
            RigidBody* b = GetBody(c.bodyB);
            if (!a || !b) continue;

            // Skip triggers — no physical response.
            if (a->isTrigger || b->isTrigger) continue;

            const float invMassSum = a->invMass + b->invMass;
            if (invMassSum < 1e-8f) continue;  // Both immovable.

            // ── Position correction (Baumgarte stabilisation) ────────────────
            if (c.depth > m_config.maxPenetration)
            {
                const float correction = (c.depth - m_config.maxPenetration)
                                        * m_config.baumgarte / invMassSum;

                if (a->type == BodyType::Dynamic)
                    a->position = a->position - c.normal * (correction * a->invMass);
                if (b->type == BodyType::Dynamic)
                    b->position = b->position + c.normal * (correction * b->invMass);
            }

            // ── Velocity impulse ─────────────────────────────────────────────
            Math::Vec3 relVel = b->velocity - a->velocity;
            float velAlongNormal = Dot(relVel, c.normal);

            // Separating — don't resolve.
            if (velAlongNormal > 0.0f) continue;

            // Restitution: use the minimum of both bodies.
            float e = std::min(a->restitution, b->restitution);

            float j = -(1.0f + e) * velAlongNormal / invMassSum;

            Math::Vec3 impulse = c.normal * j;

            if (a->type == BodyType::Dynamic)
                a->velocity = a->velocity - impulse * a->invMass;
            if (b->type == BodyType::Dynamic)
                b->velocity = b->velocity + impulse * b->invMass;

            // ── Friction impulse (tangential) ────────────────────────────────
            relVel = b->velocity - a->velocity;
            Math::Vec3 tangent = relVel - c.normal * Dot(relVel, c.normal);
            float tangentLen = Length(tangent);
            if (tangentLen > 1e-6f)
            {
                tangent = tangent * (1.0f / tangentLen);

                float jt = -Dot(relVel, tangent) / invMassSum;

                // Coulomb friction clamp.
                float mu = (a->friction + b->friction) * 0.5f;
                if (std::abs(jt) > j * mu)
                    jt = (jt > 0.0f ? 1.0f : -1.0f) * j * mu;

                Math::Vec3 frictionImpulse = tangent * jt;

                if (a->type == BodyType::Dynamic)
                    a->velocity = a->velocity - frictionImpulse * a->invMass;
                if (b->type == BodyType::Dynamic)
                    b->velocity = b->velocity + frictionImpulse * b->invMass;
            }
        }
    }
}

void PhysicsWorld::IntegratePositions(float dt)
{
    for (auto& b : m_bodies)
    {
        if (!b.active || b.type != BodyType::Dynamic) continue;

        b.position = b.position + b.velocity * dt;
    }
}

// ─── Raycasting ──────────────────────────────────────────────────────────────

RaycastHit PhysicsWorld::Raycast(const Math::Vec3& origin,
                                  const Math::Vec3& direction,
                                  float maxDistance) const
{
    Math::Vec3 dir = Normalize(direction);
    RaycastHit closest{};
    closest.distance = maxDistance;

    for (const auto& b : m_bodies)
    {
        if (!b.active) continue;

        float dist = 0.0f;
        bool hit = false;

        switch (b.shape.type)
        {
        case ShapeType::Sphere:
        {
            Math::Vec3 centre = b.position + b.shape.offset;
            hit = RaySphere(origin, dir, centre, b.shape.params.x, dist);
            break;
        }
        case ShapeType::Box:
        {
            AABB box = ComputeAABB(b);
            hit = RayAABB(origin, dir, box, dist);
            break;
        }
        case ShapeType::Capsule:
        {
            // Approximate capsule as sphere with max extent for raycasting.
            Math::Vec3 centre = b.position + b.shape.offset;
            float approxR = b.shape.params.x + b.shape.params.y;
            hit = RaySphere(origin, dir, centre, approxR, dist);
            break;
        }
        }

        if (hit && dist < closest.distance && dist >= 0.0f)
        {
            closest.body     = b.id;
            closest.distance = dist;
            closest.point    = origin + dir * dist;

            // Compute hit normal based on shape.
            if (b.shape.type == ShapeType::Sphere)
            {
                Math::Vec3 centre = b.position + b.shape.offset;
                closest.normal = Normalize(closest.point - centre);
            }
            else if (b.shape.type == ShapeType::Box)
            {
                // Approximate: find the face the hit point is closest to.
                Math::Vec3 centre = b.position + b.shape.offset;
                Math::Vec3 local = closest.point - centre;
                const auto& h = b.shape.params;

                float ax = std::abs(local.x / h.x);
                float ay = std::abs(local.y / h.y);
                float az = std::abs(local.z / h.z);

                if (ax >= ay && ax >= az)
                    closest.normal = { (local.x >= 0.0f ? 1.0f : -1.0f), 0.0f, 0.0f };
                else if (ay >= az)
                    closest.normal = { 0.0f, (local.y >= 0.0f ? 1.0f : -1.0f), 0.0f };
                else
                    closest.normal = { 0.0f, 0.0f, (local.z >= 0.0f ? 1.0f : -1.0f) };
            }
            else
            {
                Math::Vec3 centre = b.position + b.shape.offset;
                closest.normal = Normalize(closest.point - centre);
            }
        }
    }

    return closest;
}

} // namespace SagaEngine::Physics
