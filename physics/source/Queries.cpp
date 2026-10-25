#include "WorldImpl.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <unordered_set>

using namespace PhysicsInternal;

namespace
{
    class LayerFilter final : public JPH::ObjectLayerFilter
    {
    public:
        explicit LayerFilter(uint32_t layers) : m_Layers(layers) {}
        bool ShouldCollide(JPH::ObjectLayer layer) const override { return (m_Layers >> UserLayer(layer)) & 1u; }

    private:
        uint32_t m_Layers;
    };

    class EntityFilter final : public JPH::BodyFilter
    {
    public:
        explicit EntityFilter(const QueryFilter& filter) : m_Triggers(filter.triggers), m_Ignore(filter.ignore.IsValid() ? filter.ignore.RawId() : ~0ull) {}
        bool ShouldCollideLocked(const JPH::Body& body) const override
        {
            return (m_Triggers || !body.IsSensor()) && body.GetUserData() != m_Ignore;
        }

    private:
        bool m_Triggers;
        uint64_t m_Ignore;
    };

    JPH::ShapeRefC MakeQueryShape(const QueryShape& shape)
    {
        switch (shape.type)
        {
        case QueryShape::Type::Box:
        {
            const JPH::Vec3 half = ToJolt(glm::max(glm::abs(shape.halfExtents), glm::vec3(1e-3f)));
            return new JPH::BoxShape(half, std::min(JPH::cDefaultConvexRadius, 0.5f * half.ReduceMin()));
        }
        case QueryShape::Type::Capsule:
            if (shape.halfHeight > 1e-4f)
                return new JPH::CapsuleShape(shape.halfHeight, std::max(shape.radius, 1e-3f));
            [[fallthrough]];
        case QueryShape::Type::Sphere:
            break;
        }
        return new JPH::SphereShape(std::max(shape.radius, 1e-3f));
    }

    glm::vec3 Direction(const glm::vec3& direction)
    {
        const float length = glm::length(direction);
        return length > 1e-12f ? direction / length : glm::vec3(0.0f, -1.0f, 0.0f);
    }
}

bool PhysicsWorld::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& hit, const QueryFilter& filter) const
{
    const glm::vec3 dir = Direction(direction);
    const JPH::RRayCast ray(JPH::RVec3(ToJolt(origin)), ToJolt(dir * std::max(maxDistance, 0.0f)));
    JPH::RayCastResult result;
    if (!m_Impl->system->GetNarrowPhaseQuery().CastRay(ray, result, {}, LayerFilter(filter.layers), EntityFilter(filter)))
        return false;

    const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
    hit.entity = m_Impl->EntityOf(result.mBodyID);
    hit.point = ToGlm(JPH::Vec3(point));
    hit.distance = result.mFraction * maxDistance;
    hit.normal = -dir;
    JPH::BodyLockRead lock(m_Impl->system->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded())
        hit.normal = ToGlm(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
    return true;
}

std::vector<RaycastHit> PhysicsWorld::RaycastAll(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const QueryFilter& filter) const
{
    const glm::vec3 dir = Direction(direction);
    const JPH::RRayCast ray(JPH::RVec3(ToJolt(origin)), ToJolt(dir * std::max(maxDistance, 0.0f)));
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    m_Impl->system->GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings(), collector, {}, LayerFilter(filter.layers), EntityFilter(filter));
    collector.Sort();

    std::vector<RaycastHit> hits;
    std::unordered_set<uint32_t> seen;
    for (const JPH::RayCastResult& result : collector.mHits)
    {
        if (!seen.insert(result.mBodyID.GetIndexAndSequenceNumber()).second)
            continue; // the nearest hit of each body only
        const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
        RaycastHit hit;
        hit.entity = m_Impl->EntityOf(result.mBodyID);
        hit.point = ToGlm(JPH::Vec3(point));
        hit.distance = result.mFraction * maxDistance;
        hit.normal = -dir;
        JPH::BodyLockRead lock(m_Impl->system->GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded())
            hit.normal = ToGlm(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
        hits.push_back(hit);
    }
    return hits;
}

bool PhysicsWorld::ShapeCast(const QueryShape& shape, const glm::vec3& origin, const glm::quat& rotation, const glm::vec3& direction, float maxDistance, RaycastHit& hit, const QueryFilter& filter) const
{
    const glm::vec3 dir = Direction(direction);
    const JPH::ShapeRefC querying = MakeQueryShape(shape);
    const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(querying, JPH::Vec3::sOne(), JPH::RMat44::sRotationTranslation(ToJolt(rotation), JPH::RVec3(ToJolt(origin))),
                                                                       ToJolt(dir * std::max(maxDistance, 0.0f)));
    JPH::ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    m_Impl->system->GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(), collector, {}, LayerFilter(filter.layers), EntityFilter(filter));
    if (!collector.HadHit())
        return false;

    const JPH::ShapeCastResult& result = collector.mHit;
    hit.entity = m_Impl->EntityOf(result.mBodyID2);
    hit.point = ToGlm(JPH::Vec3(result.mContactPointOn2));
    const JPH::Vec3 axis = result.mPenetrationAxis;
    hit.normal = axis.LengthSq() > 1e-12f ? ToGlm(-axis.Normalized()) : -dir;
    hit.distance = result.mFraction * maxDistance;
    return true;
}

std::vector<Entity> PhysicsWorld::Overlap(const QueryShape& shape, const glm::vec3& position, const glm::quat& rotation, const QueryFilter& filter) const
{
    const JPH::ShapeRefC querying = MakeQueryShape(shape);
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    m_Impl->system->GetNarrowPhaseQuery().CollideShape(querying, JPH::Vec3::sOne(), JPH::RMat44::sRotationTranslation(ToJolt(rotation), JPH::RVec3(ToJolt(position))),
                                                       JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector, {}, LayerFilter(filter.layers), EntityFilter(filter));
    std::vector<uint64_t> ids;
    for (const JPH::CollideShapeResult& result : collector.mHits)
    {
        const Entity entity = m_Impl->EntityOf(result.mBodyID2);
        if (entity.IsValid())
            ids.push_back(entity.RawId());
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    std::vector<Entity> entities;
    for (uint64_t id : ids)
        entities.push_back(m_Impl->scene.FromId(id));
    return entities;
}

bool PhysicsWorld::AddForce(Entity entity, const glm::vec3& force)
{
    Impl::Pending* p = m_Impl->PendingFor(entity);
    if (p)
        p->force += force;
    return p != nullptr;
}

bool PhysicsWorld::AddForceAtPoint(Entity entity, const glm::vec3& force, const glm::vec3& point)
{
    Impl::Pending* p = m_Impl->PendingFor(entity);
    if (p)
        p->pointForces.emplace_back(force, point);
    return p != nullptr;
}

bool PhysicsWorld::AddTorque(Entity entity, const glm::vec3& torque)
{
    Impl::Pending* p = m_Impl->PendingFor(entity);
    if (p)
        p->torque += torque;
    return p != nullptr;
}

bool PhysicsWorld::AddImpulse(Entity entity, const glm::vec3& impulse)
{
    Impl::Pending* p = m_Impl->PendingFor(entity);
    if (p)
        p->impulse += impulse;
    return p != nullptr;
}

bool PhysicsWorld::AddImpulseAtPoint(Entity entity, const glm::vec3& impulse, const glm::vec3& point)
{
    Impl::Pending* p = m_Impl->PendingFor(entity);
    if (p)
        p->pointImpulses.emplace_back(impulse, point);
    return p != nullptr;
}

bool PhysicsWorld::AddAngularImpulse(Entity entity, const glm::vec3& impulse)
{
    Impl::Pending* p = m_Impl->PendingFor(entity);
    if (p)
        p->angularImpulse += impulse;
    return p != nullptr;
}

bool PhysicsWorld::SetVelocity(Entity entity, const glm::vec3& linear, const glm::vec3& angular)
{
    const JPH::BodyID body = m_Impl->BodyOf(entity);
    JPH::BodyInterface& bodies = m_Impl->system->GetBodyInterface();
    if (body.IsInvalid() || bodies.GetMotionType(body) == JPH::EMotionType::Static)
        return false;
    bodies.SetLinearAndAngularVelocity(body, ToJolt(linear), ToJolt(angular));
    return true;
}

bool PhysicsWorld::GetVelocity(Entity entity, glm::vec3& linear, glm::vec3& angular) const
{
    const JPH::BodyID body = m_Impl->BodyOf(entity);
    if (body.IsInvalid())
        return false;
    JPH::Vec3 l, a;
    m_Impl->system->GetBodyInterface().GetLinearAndAngularVelocity(body, l, a);
    linear = ToGlm(l);
    angular = ToGlm(a);
    return true;
}

bool PhysicsWorld::Teleport(Entity entity, const glm::vec3& position, const glm::quat& rotation)
{
    LocalTRS* trs = entity.IsValid() ? entity.Get<LocalTRS>() : nullptr;
    if (!trs)
        return false;
    SetWorldPose(m_Impl->scene, entity, *trs, position, rotation);
    const auto found = m_Impl->records.find(entity.RawId());
    if (found == m_Impl->records.end())
        return true; // its body, when it gets one, starts there
    Impl::Record& record = found->second;
    JPH::BodyInterface& bodies = m_Impl->system->GetBodyInterface();
    const glm::quat r = glm::normalize(rotation);
    for (JPH::BodyID body : { record.body, record.sensor })
        if (!body.IsInvalid())
            bodies.SetPositionAndRotation(body, JPH::RVec3(ToJolt(position)), ToJolt(r), record.motion == MotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    record.position = position;
    record.rotation = r;
    if (PhysicsBody* shown = entity.Get<PhysicsBody>())
    {
        shown->Place(position, r);
        shown->writtenVersion = trs->version;
    }
    return true;
}

bool PhysicsWorld::WakeUp(Entity entity)
{
    const JPH::BodyID body = m_Impl->BodyOf(entity);
    if (body.IsInvalid())
        return false;
    m_Impl->system->GetBodyInterface().ActivateBody(body);
    return true;
}
