#include "WorldImpl.hpp"

#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Physics/Controllers.hpp>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/RegisterTypes.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>

using namespace PhysicsInternal;

namespace
{
    // Jolt's global state (allocator hooks, the type factory, the debug renderer) is set up by the
    // first world and torn down by the last.
    int g_JoltWorlds = 0;

    void JoltTrace(const char* format, ...)
    {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Logger::Info(std::string("Jolt: ") + buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    bool JoltAssertFailed(const char* expression, const char* message, const char* file, JPH::uint line)
    {
        Logger::Error(std::string("Jolt assert ") + file + ":" + std::to_string(line) + ": " + expression + (message ? std::string(" ") + message : std::string()));
        return false;
    }
#endif

    void AcquireJolt()
    {
        if (g_JoltWorlds++ > 0)
            return;
        JPH::RegisterDefaultAllocator();
        JPH::Trace = JoltTrace;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed;)
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        AcquireDebugRenderer();
    }

    void ReleaseJolt()
    {
        if (--g_JoltWorlds > 0)
            return;
        ReleaseDebugRenderer();
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    // FNV-1a over what a body is made from: when it changes, the body is made again.
    struct Hasher
    {
        uint64_t value = 1469598103934665603ull;
        void Bytes(const void* data, size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (size_t i = 0; i < size; ++i)
                value = (value ^ bytes[i]) * 1099511628211ull;
        }
        template <typename T>
        void Add(const T& v) { Bytes(&v, sizeof(T)); }
    };

    void HashGeometry(Hasher& h, const ColliderGeometry& g)
    {
        h.Add(g.shape);
        h.Add(g.position);
        h.Add(g.rotation);
        h.Add(g.halfExtents);
        h.Add(g.radius);
        h.Add(g.halfHeight);
        h.Add(g.mesh.get());
        h.Add(g.scale);
        h.Add(g.collider->friction);
        h.Add(g.collider->restitution);
        h.Add(g.collider->density);
        h.Add(g.collider->isTrigger);
    }

    void HashBody(Hasher& h, const RigidBody& b)
    {
        h.Add(b.motion);
        h.Add(b.mass);
        h.Add(b.linearDamping);
        h.Add(b.angularDamping);
        h.Add(b.gravityFactor);
        h.Add(b.continuous);
        h.Add(b.allowSleeping);
        h.Add(b.lockedAxes);
        h.Add(b.layer);
        h.Add(b.centerOfMass);
    }

    JPH::EMotionType ToJolt(MotionType motion)
    {
        return motion == MotionType::Dynamic ? JPH::EMotionType::Dynamic : motion == MotionType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static;
    }
}

namespace PhysicsInternal
{
    glm::mat4 LocalMatrix(const LocalTRS& t)
    {
        glm::mat4 m = glm::mat4_cast(glm::quat(t.rot_quat[3], t.rot_quat[0], t.rot_quat[1], t.rot_quat[2]));
        m[0] *= t.scl[0];
        m[1] *= t.scl[1];
        m[2] *= t.scl[2];
        m[3] = glm::vec4(t.pos[0], t.pos[1], t.pos[2], 1.0f);
        return m;
    }

    glm::mat4 WorldMatrix(const Scene& scene, Entity entity)
    {
        glm::mat4 m(1.0f);
        for (Entity e = entity; e.IsValid(); e = scene.Parent(e))
            if (const LocalTRS* t = e.TryGet<LocalTRS>())
                m = LocalMatrix(*t) * m;
        return m;
    }

    void SplitPose(const glm::mat4& m, glm::vec3& position, glm::quat& rotation)
    {
        position = glm::vec3(m[3]);
        glm::mat3 axes(glm::normalize(glm::vec3(m[0])), glm::normalize(glm::vec3(m[1])), glm::normalize(glm::vec3(m[2])));
        if (glm::determinant(axes) < 0.0f)
            axes[0] = -axes[0];
        rotation = glm::normalize(glm::quat_cast(axes));
    }

    void SetWorldPose(const Scene& scene, Entity entity, LocalTRS& trs, glm::vec3 position, glm::quat rotation)
    {
        const Entity parent = scene.Parent(entity);
        if (parent.IsValid())
        {
            // Into the parent's space (its scale left out of the rotation).
            const glm::mat4 parentWorld = WorldMatrix(scene, parent);
            const glm::mat4 local = glm::inverse(parentWorld) * (glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation));
            SplitPose(local, position, rotation);
        }

        const float values[7] = { position.x, position.y, position.z, rotation.x, rotation.y, rotation.z, rotation.w };
        float* targets[7] = { &trs.pos[0], &trs.pos[1], &trs.pos[2], &trs.rot_quat[0], &trs.rot_quat[1], &trs.rot_quat[2], &trs.rot_quat[3] };
        bool changed = false;
        for (int i = 0; i < 7; ++i)
        {
            if (*targets[i] != values[i])
            {
                *targets[i] = values[i];
                changed = true;
            }
        }
        if (changed)
            ++trs.version;
    }

    void ContactCollector::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings&)
    {
        Added contact;
        contact.body1 = body1.GetID();
        contact.body2 = body2.GetID();
        contact.subShapes = SubShapePairKey(manifold.mSubShapeID1, manifold.mSubShapeID2);
        // The trigger first; the manifold's normal points from body 1 to body 2.
        const bool flip = body2.IsSensor() && !body1.IsSensor();
        const JPH::Body& a = flip ? body2 : body1;
        const JPH::Body& b = flip ? body1 : body2;
        contact.entityA = a.GetUserData();
        contact.entityB = b.GetUserData();
        contact.trigger = a.IsSensor() || b.IsSensor();
        const JPH::Vec3 normal = flip ? -manifold.mWorldSpaceNormal : manifold.mWorldSpaceNormal;
        const JPH::RVec3 point = manifold.GetWorldSpaceContactPointOn1(0);
        contact.point = ToGlm(JPH::Vec3(point));
        contact.normal = ToGlm(normal);
        contact.speed = (a.GetPointVelocity(point) - b.GetPointVelocity(point)).Dot(normal);
        std::lock_guard lock(m_Mutex);
        m_Added.push_back(contact);
    }

    void ContactCollector::OnContactRemoved(const JPH::SubShapeIDPair& pair)
    {
        std::lock_guard lock(m_Mutex);
        m_Removed.push_back(Removed{ pair.GetBody1ID(), pair.GetBody2ID(), SubShapePairKey(pair.GetSubShapeID1(), pair.GetSubShapeID2()) });
    }

    void ContactCollector::Add(const Added& contact)
    {
        std::lock_guard lock(m_Mutex);
        m_Added.push_back(contact);
    }

    void ContactCollector::Remove(const Removed& contact)
    {
        std::lock_guard lock(m_Mutex);
        m_Removed.push_back(contact);
    }

    void ContactCollector::Take(std::vector<Added>& added, std::vector<Removed>& removed)
    {
        {
            std::lock_guard lock(m_Mutex);
            added.swap(m_Added);
            removed.swap(m_Removed);
            m_Added.clear();
            m_Removed.clear();
        }
        std::sort(added.begin(), added.end(), [](const Added& l, const Added& r) {
            return std::tuple(l.body1.GetIndexAndSequenceNumber(), l.body2.GetIndexAndSequenceNumber(), l.subShapes) <
                   std::tuple(r.body1.GetIndexAndSequenceNumber(), r.body2.GetIndexAndSequenceNumber(), r.subShapes);
        });
        std::sort(removed.begin(), removed.end(), [](const Removed& l, const Removed& r) {
            return std::tuple(l.body1.GetIndexAndSequenceNumber(), l.body2.GetIndexAndSequenceNumber(), l.subShapes) <
                   std::tuple(r.body1.GetIndexAndSequenceNumber(), r.body2.GetIndexAndSequenceNumber(), r.subShapes);
        });
    }

    void ContactCollector::Clear()
    {
        std::lock_guard lock(m_Mutex);
        m_Added.clear();
        m_Removed.clear();
    }
}

PhysicsWorld::Impl::Impl(Scene& scene, const PhysicsSettings& initial) : scene(scene), settings(initial)
{
    AcquireJolt();
    temp = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
#if defined(__EMSCRIPTEN__)
    jobs = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
#else
    jobs = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers);
#endif
    CreateSystem();
}

PhysicsWorld::Impl::~Impl()
{
    RemoveAll();
    system.reset();
    characterCollision.reset();
    jointFilter = nullptr;
    shapes = ShapeCache();
    jobs.reset();
    temp.reset();
    ReleaseJolt();
}

// A new system each time the bodies are all dropped: one with no history (its broad phase, body
// ids, caches) simulates the same scene the same way every time.
void PhysicsWorld::Impl::CreateSystem()
{
    system = std::make_unique<JPH::PhysicsSystem>();
    system->Init(65536, 0, 65536, 16384, broadPhase, objectVsBroadPhase, objectPairs);
    system->SetCombineFriction(CombineFriction);
    system->SetCombineRestitution(CombineRestitution);
    system->SetContactListener(&contacts);
    characterCollision = std::make_unique<JPH::CharacterVsCharacterCollisionSimple>();
    if (!jointFilter)
        jointFilter = new JointCollisionFilter();
    characterContacts.collector = &contacts;
    Apply();
}

void PhysicsWorld::Impl::Apply()
{
    system->SetGravity(PhysicsInternal::ToJolt(settings.gravity));
    JPH::PhysicsSettings joltSettings = system->GetPhysicsSettings();
    joltSettings.mPenetrationSlop = std::max(settings.penetrationSlop, 0.0f);
    joltSettings.mSpeculativeContactDistance = std::max(settings.speculativeContactDistance, 0.0f);
    system->SetPhysicsSettings(joltSettings);
    objectPairs.Set(settings);
}

void PhysicsWorld::Impl::RemoveBody(JPH::BodyID& id)
{
    if (id.IsInvalid())
        return;
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    bodies.RemoveBody(id);
    bodies.DestroyBody(id);
    removedBodies.push_back(id.GetIndexAndSequenceNumber());
    id = JPH::BodyID();
}

void PhysicsWorld::Impl::RemoveAll()
{
    for (auto& [id, softBody] : softBodies)
        RemoveSoftBody(id, softBody);
    softBodies.clear();
    for (auto& [id, ragdoll] : ragdolls)
        RemoveRagdoll(ragdoll);
    ragdolls.clear();
    ragdollBones.clear();
    for (auto& [id, joint] : joints)
        RemoveJoint(joint);
    joints.clear();
    jointFilter->pairs.clear();
    brokenJoints.clear();
    for (auto& [id, character] : characters)
        RemoveCharacter(character);
    characters.clear();
    for (auto& [id, vehicle] : vehicles)
        RemoveVehicle(vehicle);
    vehicles.clear();
    wheelRests.clear();
    for (auto& [id, record] : records)
    {
        RemoveBody(record.body);
        RemoveBody(record.sensor);
    }
    records.clear();
    pending.clear();
    bodyEntities.clear();
    removedBodies.clear();
    touching.clear();
    events.clear();
    contactLines.clear();
    contacts.Clear();
    accumulator = 0.0f;
    waterTime = 0.0f;
}

// Every body the scene asks for, from its hierarchy. Shapes are placed relative to their body from
// the LocalTRS values below it (and the scale above it), so they don't change as it moves.
void PhysicsWorld::Impl::Collect()
{
    candidates.clear();
    waters.clear();
    std::function<void(Entity, const glm::mat4&, const glm::vec3&, int, const glm::mat4&)> walk =
        [&](Entity e, const glm::mat4& parentWorld, const glm::vec3& parentScale, int owner, const glm::mat4& parentRelative) {
            if (e.TryGet<VehicleWheel>() || e.TryGet<CharacterController>())
                return; // a vehicle's wheel or a character (and what is on them) is no body
            const LocalTRS* trs = e.TryGet<LocalTRS>();
            const glm::mat4 local = trs ? LocalMatrix(*trs) : glm::mat4(1.0f);
            const glm::mat4 world = parentWorld * local;
            const glm::vec3 scale = trs ? parentScale * glm::vec3(trs->scl[0], trs->scl[1], trs->scl[2]) : parentScale;
            glm::mat4 relative = parentRelative * local;
            if (const WaterVolume* water = e.TryGet<WaterVolume>())
                AddWater(e, world, *water);

            if (const RigidBody* body = e.TryGet<RigidBody>())
            {
                Candidate candidate;
                candidate.owner = e;
                candidate.hasBody = true;
                candidate.body = *body;
                candidate.world = world;
                candidates.push_back(std::move(candidate));
                owner = static_cast<int>(candidates.size()) - 1;
                relative = glm::scale(glm::mat4(1.0f), scale);
            }

            if (HasCollider(e))
            {
                int target = owner;
                glm::mat4 placement = relative;
                if (target < 0)
                {
                    Candidate candidate;
                    candidate.owner = e;
                    candidate.world = world;
                    candidates.push_back(std::move(candidate));
                    target = static_cast<int>(candidates.size()) - 1;
                    placement = glm::scale(glm::mat4(1.0f), scale);
                }
                ForEachCollider(e, placement, [&](const ColliderGeometry& g) {
                    (g.collider->isTrigger ? candidates[target].triggers : candidates[target].solid).push_back(g);
                });
            }

            e.ForEachChild([&](Entity child) { walk(child, world, scale, owner, relative); });
        };
    scene.ForEachRoot([&](Entity root) { walk(root, glm::mat4(1.0f), glm::vec3(1.0f), -1, glm::mat4(1.0f)); });
    // Bodies are made in the order of their entities, not of the scene's storage (which a restored
    // scene may have shuffled).
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.owner.RawId() < b.owner.RawId(); });
}

JPH::BodyID PhysicsWorld::Impl::Create(const Candidate& c, std::vector<ColliderGeometry> parts, bool sensor, const glm::vec3& position, const glm::quat& rotation)
{
    MotionType motion = c.hasBody ? c.body.motion : MotionType::Static;
    uint8_t locked = c.hasBody ? c.body.lockedAxes : 0;
    if (motion == MotionType::Dynamic && (locked & 63) == 63)
        motion = MotionType::Kinematic; // nothing left to move: Jolt can't simulate that
    // The triggers of a body with solid colliders follow it.
    if (sensor && motion == MotionType::Dynamic)
        motion = MotionType::Kinematic;

    bool hasMesh = false;
    for (ColliderGeometry& g : parts)
    {
        if (g.shape != ColliderShape::Mesh)
            continue;
        if (motion == MotionType::Dynamic)
        {
            // Jolt can't simulate a moving triangle mesh against others: its hull will do.
            g.shape = ColliderShape::ConvexHull;
            if (warnedDynamicMesh.insert(c.owner.RawId()).second)
                Logger::Warning(std::string("Physics: ") + c.owner.GetName() + " is dynamic, so its mesh collider is used as a convex hull");
        }
        else
        {
            hasMesh = true;
        }
    }

    JPH::ShapeRefC shape = MakeShape(parts, shapes);
    if (!shape)
        return JPH::BodyID();
    if (c.hasBody && motion == MotionType::Dynamic && c.body.centerOfMass != glm::vec3(0.0f))
    {
        JPH::OffsetCenterOfMassShapeSettings offset(PhysicsInternal::ToJolt(c.body.centerOfMass), shape);
        offset.SetEmbedded();
        const JPH::ShapeSettings::ShapeResult result = offset.Create();
        if (result.IsValid())
            shape = result.Get();
    }

    JPH::BodyCreationSettings s(shape, PhysicsInternal::ToJolt(position), PhysicsInternal::ToJolt(rotation), ToJolt(motion), MakeLayer(c.hasBody ? c.body.layer : 0, motion != MotionType::Static));
    s.mUserData = c.owner.RawId();
    s.mIsSensor = parts.front().collider->isTrigger;
    s.mCollideKinematicVsNonDynamic = s.mIsSensor; // triggers notice static and kinematic bodies too
    s.mFriction = parts.front().collider->friction;
    s.mRestitution = parts.front().collider->restitution;
    if (hasMesh && motion == MotionType::Kinematic)
    {
        // A triangle mesh has no volume to weigh; a kinematic body still needs some mass.
        s.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
        s.mMassPropertiesOverride.mMass = 1.0f;
        s.mMassPropertiesOverride.mInertia = JPH::Mat44::sIdentity();
    }
    if (c.hasBody)
    {
        const RigidBody& b = c.body;
        s.mLinearDamping = std::max(b.linearDamping, 0.0f);
        s.mAngularDamping = std::max(b.angularDamping, 0.0f);
        s.mGravityFactor = b.gravityFactor;
        s.mMotionQuality = b.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
        s.mAllowSleeping = b.allowSleeping;
        if (motion == MotionType::Dynamic)
        {
            s.mAllowedDOFs = static_cast<JPH::EAllowedDOFs>(63 & ~locked);
            if (b.mass > 0.0f)
            {
                s.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                s.mMassPropertiesOverride.mMass = b.mass;
            }
            s.mLinearVelocity = PhysicsInternal::ToJolt(b.startVelocity);
            s.mAngularVelocity = PhysicsInternal::ToJolt(b.startAngularVelocity);
        }
    }
    const JPH::BodyID id = system->GetBodyInterface().CreateAndAddBody(s, motion == MotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    if (!id.IsInvalid())
        bodyEntities[id.GetIndexAndSequenceNumber()] = c.owner.RawId();
    return id;
}

void PhysicsWorld::Impl::Sync()
{
    Collect();
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    for (auto& [id, record] : records)
        record.seen = false;

    size_t created = 0;
    for (const Candidate& c : candidates)
    {
        if (c.solid.empty() && c.triggers.empty())
            continue;

        Hasher hash;
        hash.Add(c.hasBody);
        if (c.hasBody)
            HashBody(hash, c.body);
        for (const ColliderGeometry& g : c.solid)
            HashGeometry(hash, g);
        for (const ColliderGeometry& g : c.triggers)
            HashGeometry(hash, g);

        glm::vec3 position;
        glm::quat rotation;
        SplitPose(c.world, position, rotation);
        const LocalTRS* trs = c.owner.TryGet<LocalTRS>();

        auto found = records.find(c.owner.RawId());
        // A body made again keeps moving as it did.
        bool keepVelocity = false;
        JPH::Vec3 linearVelocity = JPH::Vec3::sZero(), angularVelocity = JPH::Vec3::sZero();
        if (found != records.end() && found->second.signature != hash.value)
        {
            if (found->second.motion == MotionType::Dynamic && !found->second.body.IsInvalid())
            {
                keepVelocity = true;
                bodies.GetLinearAndAngularVelocity(found->second.body, linearVelocity, angularVelocity);
            }
            if (auto vehicle = vehicles.find(c.owner.RawId()); vehicle != vehicles.end())
            {
                RemoveVehicle(vehicle->second); // made again on the new body below
                vehicles.erase(vehicle);
            }
            DropJointsOf(found->second.body);
            RemoveBody(found->second.body);
            RemoveBody(found->second.sensor);
            records.erase(found);
            found = records.end();
        }

        if (found == records.end())
        {
            Record record;
            record.signature = hash.value;
            record.motion = c.hasBody ? c.body.motion : MotionType::Static;
            record.position = position;
            record.rotation = rotation;
            record.writtenVersion = trs ? trs->version : ~0u;
            if (!c.solid.empty())
            {
                record.body = Create(c, c.solid, false, position, rotation);
                if (!c.triggers.empty())
                    record.sensor = Create(c, c.triggers, true, position, rotation);
            }
            else
            {
                record.body = Create(c, c.triggers, false, position, rotation);
            }
            if (keepVelocity && record.motion == MotionType::Dynamic && !record.body.IsInvalid())
                bodies.SetLinearAndAngularVelocity(record.body, linearVelocity, angularVelocity);
            record.interpolate = !c.hasBody || c.body.interpolate;
            record.Place(position, rotation);
            record.seen = true;
            records.emplace(c.owner.RawId(), record);
            ++created;
            continue;
        }

        Record& record = found->second;
        record.seen = true;
        record.interpolate = !c.hasBody || c.body.interpolate;
        switch (record.motion)
        {
        case MotionType::Static:
            // Moved by the editor or by code.
            if (position != record.position || rotation != record.rotation)
            {
                bodies.SetPositionAndRotation(record.body, PhysicsInternal::ToJolt(position), PhysicsInternal::ToJolt(rotation), JPH::EActivation::DontActivate);
                if (!record.sensor.IsInvalid())
                    bodies.SetPositionAndRotation(record.sensor, PhysicsInternal::ToJolt(position), PhysicsInternal::ToJolt(rotation), JPH::EActivation::DontActivate);
            }
            break;
        case MotionType::Kinematic:
            break; // moved toward the entity each step
        case MotionType::Dynamic:
            // Moved by something other than the simulation: put the body there.
            if (trs && trs->version != record.writtenVersion)
            {
                bodies.SetPositionAndRotation(record.body, PhysicsInternal::ToJolt(position), PhysicsInternal::ToJolt(rotation), JPH::EActivation::Activate);
                record.writtenVersion = trs->version;
                record.Place(position, rotation);
            }
            break;
        }
        record.position = position;
        record.rotation = rotation;
    }

    for (auto it = records.begin(); it != records.end();)
    {
        if (it->second.seen)
        {
            ++it;
            continue;
        }
        if (auto vehicle = vehicles.find(it->first); vehicle != vehicles.end())
        {
            RemoveVehicle(vehicle->second);
            vehicles.erase(vehicle);
        }
        DropJointsOf(it->second.body);
        RemoveBody(it->second.body);
        RemoveBody(it->second.sensor);
        pending.erase(it->first);
        it = records.erase(it);
    }

    if (created > 16)
        system->OptimizeBroadPhase();
    SyncVehicles();
    SyncCharacters();
    SyncJoints();
    SyncRagdolls();
    SyncSoftBodies();
}

void PhysicsWorld::Impl::MoveKinematics(float dt)
{
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    for (auto& [id, record] : records)
    {
        if (record.motion == MotionType::Kinematic && !record.body.IsInvalid())
            bodies.MoveKinematic(record.body, PhysicsInternal::ToJolt(record.position), PhysicsInternal::ToJolt(record.rotation), dt);
    }
}

// Jolt clears forces after every step, so they are given again before each one.
void PhysicsWorld::Impl::ApplyPending(bool impulses)
{
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    for (const auto& [id, p] : pending)
    {
        const auto found = records.find(id);
        if (found == records.end() || found->second.motion != MotionType::Dynamic || found->second.body.IsInvalid())
            continue;
        const JPH::BodyID body = found->second.body;
        if (p.force != glm::vec3(0.0f))
            bodies.AddForce(body, PhysicsInternal::ToJolt(p.force));
        if (p.torque != glm::vec3(0.0f))
            bodies.AddTorque(body, PhysicsInternal::ToJolt(p.torque));
        for (const auto& [force, point] : p.pointForces)
            bodies.AddForce(body, PhysicsInternal::ToJolt(force), JPH::RVec3(PhysicsInternal::ToJolt(point)));
        if (!impulses)
            continue;
        if (p.impulse != glm::vec3(0.0f))
            bodies.AddImpulse(body, PhysicsInternal::ToJolt(p.impulse));
        if (p.angularImpulse != glm::vec3(0.0f))
            bodies.AddAngularImpulse(body, PhysicsInternal::ToJolt(p.angularImpulse));
        for (const auto& [impulse, point] : p.pointImpulses)
            bodies.AddImpulse(body, PhysicsInternal::ToJolt(impulse), JPH::RVec3(PhysicsInternal::ToJolt(point)));
    }
}

// Jolt's per sub-shape contacts become one Begin and one End per pair of bodies. Jolt also drops the
// contacts of a body that falls asleep; those are kept until it wakes and they aren't found again.
void PhysicsWorld::Impl::ProcessContacts()
{
    std::vector<ContactCollector::Added> added;
    std::vector<ContactCollector::Removed> removed;
    contacts.Take(added, removed);
    const JPH::BodyInterface& bodies = system->GetBodyInterface();
    auto sleeping = [&](JPH::BodyID id) { return bodies.IsAdded(id) && !bodies.IsActive(id) && bodies.GetMotionType(id) != JPH::EMotionType::Static; };
    auto awake = [&](JPH::BodyID id) { return bodies.IsAdded(id) && (bodies.IsActive(id) || bodies.GetMotionType(id) == JPH::EMotionType::Static); };

    for (const ContactCollector::Added& c : added)
    {
        Touching& t = touching[BodyPairKey(c.body1, c.body2)];
        if (t.Empty())
        {
            t.body1 = c.body1;
            t.body2 = c.body2;
            t.a = c.entityA;
            t.b = c.entityB;
            t.trigger = c.trigger;
            ContactEvent e;
            e.type = ContactEvent::Type::Begin;
            e.a = scene.FromId(c.entityA);
            e.b = scene.FromId(c.entityB);
            e.trigger = c.trigger;
            e.point = c.point;
            e.normal = c.normal;
            e.speed = c.speed;
            events.push_back(e);
        }
        t.asleep.erase(c.subShapes);
        t.live.insert(c.subShapes);
    }

    auto end = [&](const Touching& t) {
        ContactEvent e;
        e.type = ContactEvent::Type::End;
        e.a = scene.FromId(t.a);
        e.b = scene.FromId(t.b);
        e.trigger = t.trigger;
        events.push_back(e);
    };
    for (const ContactCollector::Removed& r : removed)
    {
        const auto found = touching.find(BodyPairKey(r.body1, r.body2));
        if (found == touching.end())
            continue;
        Touching& t = found->second;
        if (t.live.erase(r.subShapes) && (sleeping(r.body1) || sleeping(r.body2)))
            t.asleep[r.subShapes] = 0;
        if (t.Empty())
        {
            end(t);
            touching.erase(found);
        }
    }

    // Contacts kept through a sleep: once both bodies have been awake for two steps without Jolt
    // finding them again, they are over. Bodies removed before this step end theirs now.
    std::unordered_set<uint32_t> gone(removedBodies.begin(), removedBodies.end());
    std::vector<uint64_t> ended;
    for (auto& [key, t] : touching)
    {
        if (gone.count(t.body1.GetIndexAndSequenceNumber()) || gone.count(t.body2.GetIndexAndSequenceNumber()))
        {
            ended.push_back(key);
            continue;
        }
        if (t.asleep.empty() || !awake(t.body1) || !awake(t.body2))
            continue;
        for (auto it = t.asleep.begin(); it != t.asleep.end();)
            it = ++it->second >= 2 ? t.asleep.erase(it) : std::next(it);
        if (t.Empty())
            ended.push_back(key);
    }
    std::sort(ended.begin(), ended.end());
    for (uint64_t key : ended)
    {
        end(touching[key]);
        touching.erase(key);
    }
    for (uint32_t id : removedBodies)
        bodyEntities.erase(id);
    removedBodies.clear();
}

void PhysicsWorld::Impl::TrackPoses()
{
    JPH::BodyInterface& bodies = system->GetBodyInterface();
    for (auto& [id, record] : records)
    {
        if (!record.sensor.IsInvalid())
        {
            // Triggers follow their bodies.
            JPH::RVec3 p;
            JPH::Quat q;
            bodies.GetPositionAndRotation(record.body, p, q);
            bodies.SetPositionAndRotation(record.sensor, p, q, JPH::EActivation::DontActivate);
        }
        if (record.motion != MotionType::Dynamic || record.body.IsInvalid())
            continue;
        record.previousPosition = record.currentPosition;
        record.previousRotation = record.currentRotation;
        if (!bodies.IsActive(record.body))
            continue;
        JPH::RVec3 p;
        JPH::Quat q;
        bodies.GetPositionAndRotation(record.body, p, q);
        record.currentPosition = ToGlm(p);
        record.currentRotation = ToGlm(q);
    }
    for (auto& [id, record] : characters)
    {
        if (!record.character)
            continue;
        const glm::vec3 position = ToGlm(JPH::Vec3(record.character->GetPosition()));
        record.previousPosition = record.tracked ? record.currentPosition : position;
        record.previousYaw = record.tracked ? record.currentYaw : record.yaw;
        record.currentPosition = position;
        record.currentYaw = record.yaw;
        record.tracked = true;
    }
    for (auto& [id, ragdoll] : ragdolls)
        if (ragdoll.ragdoll)
            TrackRagdoll(ragdoll, false);
    TrackSoftBodies();
}

// Dynamic bodies move their entities (in their parent's space, parents first so a nested body is
// placed under its parent's new pose).
void PhysicsWorld::Impl::WriteBack(float alpha)
{
    std::vector<std::pair<int, uint64_t>> moved; // (depth, entity)
    for (auto& [id, record] : records)
    {
        if (record.motion != MotionType::Dynamic || record.body.IsInvalid())
            continue;
        const float a = record.interpolate ? alpha : 1.0f;
        const glm::vec3 position = glm::mix(record.previousPosition, record.currentPosition, a);
        const glm::quat rotation = glm::normalize(glm::slerp(record.previousRotation, record.currentRotation, a));
        if (position == record.shownPosition && rotation == record.shownRotation)
            continue;
        record.shownPosition = position;
        record.shownRotation = rotation;
        int depth = 0;
        for (Entity e = scene.Parent(scene.FromId(id)); e.IsValid(); e = scene.Parent(e))
            ++depth;
        moved.emplace_back(depth, id);
    }
    std::sort(moved.begin(), moved.end());

    for (const auto& [depth, id] : moved)
    {
        Record& record = records[id];
        Entity entity = scene.FromId(id);
        LocalTRS* trs = entity.IsValid() ? entity.Get<LocalTRS>() : nullptr;
        if (!trs)
            continue;
        SetWorldPose(scene, entity, *trs, record.shownPosition, record.shownRotation);
        record.writtenVersion = trs->version;
    }
}

void PhysicsWorld::Impl::Step(float dt)
{
    events.clear();
    brokenJoints.clear();
    Sync();
    if (dt <= 0.0f)
        return;

    const float step = std::max(settings.fixedTimestep, 1e-4f);
    accumulator += dt;
    int steps = 0;
    // A small tolerance so a frame of exactly one step (the editor's Step button) runs one.
    while (accumulator + 1e-6f >= step && steps < settings.maxStepsPerFrame)
    {
        if (steps == 0 && drawContacts)
        {
            contactLines.clear();
            BeginStepDrawing(&contactLines, maxContactLines);
        }
        MoveKinematics(step);
        ApplyPending(steps == 0);
        DriveVehicles();
        MoveCharacters(step);
        DriveJoints();
        ApplyBuoyancy(step);
        DriveRagdolls(step);
        DriveSoftBodies(step);
        system->Update(step, std::max(settings.collisionSteps, 1), temp.get(), jobs.get());
        waterTime += step;
        TrackPoses();
        const size_t eventsBefore = events.size();
        ProcessContacts();
        CheckRagdollImpacts(eventsBefore);
        CheckJoints(step);
        accumulator -= step;
        ++steps;
    }
    if (drawContacts)
        EndStepDrawing();
    if (steps == settings.maxStepsPerFrame && accumulator >= step)
        accumulator = 0.0f; // too far behind: let the simulation run slow instead of piling up
    accumulator = std::max(accumulator, 0.0f);
    if (steps > 0)
    {
        pending.clear();
        RippleWater();
    }
    // Frames between steps show the bodies moving on too.
    const float alpha = settings.interpolation ? std::clamp(accumulator / step, 0.0f, 1.0f) : 1.0f;
    WriteBack(alpha);
    WriteWheels();
    WriteCharacters(alpha, steps > 0);
    WriteRagdolls(alpha);
    WriteSoftBodies(alpha);
}

JPH::BodyID PhysicsWorld::Impl::BodyOf(Entity entity) const
{
    const auto found = records.find(entity.RawId());
    return found == records.end() ? JPH::BodyID() : found->second.body;
}

Entity PhysicsWorld::Impl::EntityOf(JPH::BodyID body) const
{
    const auto found = bodyEntities.find(body.GetIndexAndSequenceNumber());
    return found == bodyEntities.end() ? Entity{} : scene.FromId(found->second);
}

PhysicsWorld::Impl::Pending* PhysicsWorld::Impl::PendingFor(Entity entity)
{
    const RigidBody* body = entity.IsValid() ? entity.TryGet<RigidBody>() : nullptr;
    if (!body || body->motion != MotionType::Dynamic || (body->lockedAxes & 63) == 63)
        return nullptr;
    return &pending[entity.RawId()];
}

void PhysicsWorld::RegisterComponents()
{
    ECS::RegisterComponent<RigidBody>("RigidBody");
    ECS::RegisterComponent<BoxCollider>("BoxCollider");
    ECS::RegisterComponent<SphereCollider>("SphereCollider");
    ECS::RegisterComponent<CapsuleCollider>("CapsuleCollider");
    ECS::RegisterComponent<CylinderCollider>("CylinderCollider");
    ECS::RegisterComponent<MeshCollider>("MeshCollider");
    ECS::RegisterComponent<Vehicle>("Vehicle");
    ECS::RegisterComponent<VehicleWheel>("VehicleWheel");
    ECS::RegisterComponent<PlayerControl>("PlayerControl");
    ECS::RegisterComponent<FollowCamera>("FollowCamera");
    ECS::RegisterComponent<CharacterController>("CharacterController");
    ECS::RegisterComponent<Joint>("Joint");
    ECS::RegisterComponent<WaterVolume>("WaterVolume");
    ECS::RegisterComponent<Ragdoll>("Ragdoll");
    ECS::RegisterComponent<SoftBody>("SoftBody");
    RegisterSerializers();
}

PhysicsWorld::PhysicsWorld(Scene& scene, const PhysicsSettings& settings)
    : m_Impl(std::make_unique<Impl>(scene, settings))
{
}

PhysicsWorld::~PhysicsWorld() = default;

const PhysicsSettings& PhysicsWorld::Settings() const
{
    return m_Impl->settings;
}

void PhysicsWorld::SetSettings(const PhysicsSettings& settings)
{
    m_Impl->settings = settings;
    m_Impl->Apply();
}

void PhysicsWorld::Step(float dt)
{
    m_Impl->Step(dt);
}

void PhysicsWorld::ShowLatest()
{
    m_Impl->WriteBack(1.0f);
    m_Impl->WriteWheels();
    m_Impl->WriteCharacters(1.0f, false);
    m_Impl->WriteRagdolls(1.0f);
    m_Impl->WriteSoftBodies(1.0f);
}

void PhysicsWorld::Reset()
{
    m_Impl->RemoveAll();
    m_Impl->CreateSystem();
    m_Impl->shapes.Prune();
}

size_t PhysicsWorld::BodyCount() const
{
    return m_Impl->system->GetNumBodies();
}

size_t PhysicsWorld::ActiveBodyCount() const
{
    return m_Impl->system->GetNumActiveBodies(JPH::EBodyType::RigidBody);
}

bool PhysicsWorld::GetBodyInfo(Entity entity, BodyInfo& info) const
{
    const auto found = m_Impl->records.find(entity.RawId());
    if (found == m_Impl->records.end() || found->second.body.IsInvalid())
        return false;

    JPH::BodyLockRead lock(m_Impl->system->GetBodyLockInterface(), found->second.body);
    if (!lock.Succeeded())
        return false;
    const JPH::Body& body = lock.GetBody();
    info.motion = found->second.motion;
    info.sleeping = !body.IsActive();
    info.sensor = body.IsSensor();
    info.linearVelocity = ToGlm(body.GetLinearVelocity());
    info.angularVelocity = ToGlm(body.GetAngularVelocity());
    const JPH::MotionProperties* motion = body.GetMotionPropertiesUnchecked();
    info.mass = body.IsDynamic() && motion && motion->GetInverseMass() > 0.0f ? 1.0f / motion->GetInverseMass() : 0.0f;
    return true;
}

const std::vector<ContactEvent>& PhysicsWorld::Events() const
{
    return m_Impl->events;
}

bool PhysicsWorld::IsTouching(Entity a, Entity b) const
{
    for (const auto& [key, t] : m_Impl->touching)
        if ((t.a == a.RawId() && t.b == b.RawId()) || (t.a == b.RawId() && t.b == a.RawId()))
            return true;
    return false;
}
