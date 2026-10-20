#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include "JoltCommon.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

using namespace PhysicsInternal;

namespace
{
    // Jolt's global state (allocator hooks, the type factory) is set up by the first world and torn
    // down by the last.
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
    }

    void ReleaseJolt()
    {
        if (--g_JoltWorlds > 0)
            return;
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    glm::mat4 LocalMatrix(const LocalTRS& t)
    {
        glm::mat4 m = glm::mat4_cast(glm::quat(t.rot_quat[3], t.rot_quat[0], t.rot_quat[1], t.rot_quat[2]));
        m[0] *= t.scl[0];
        m[1] *= t.scl[1];
        m[2] *= t.scl[2];
        m[3] = glm::vec4(t.pos[0], t.pos[1], t.pos[2], 1.0f);
        return m;
    }

    // The entity's world transform from the LocalTRS chain (WorldXform may not be updated yet for
    // entities made this frame).
    glm::mat4 WorldMatrix(const Scene& scene, Entity entity)
    {
        glm::mat4 m(1.0f);
        for (Entity e = entity; e.IsValid(); e = scene.Parent(e))
            if (const LocalTRS* t = e.TryGet<LocalTRS>())
                m = LocalMatrix(*t) * m;
        return m;
    }

    // Translation and rotation of a transform, without its scale.
    void SplitPose(const glm::mat4& m, glm::vec3& position, glm::quat& rotation)
    {
        position = glm::vec3(m[3]);
        glm::mat3 axes(glm::normalize(glm::vec3(m[0])), glm::normalize(glm::vec3(m[1])), glm::normalize(glm::vec3(m[2])));
        if (glm::determinant(axes) < 0.0f)
            axes[0] = -axes[0];
        rotation = glm::normalize(glm::quat_cast(axes));
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
    }

    JPH::ShapeRefC Created(const JPH::ShapeSettings::ShapeResult& result)
    {
        if (result.HasError())
        {
            Logger::Warning(std::string("Physics: a shape couldn't be made: ") + result.GetError().c_str());
            return nullptr;
        }
        return result.Get();
    }

    JPH::ShapeRefC MakePrimitive(const ColliderGeometry& g)
    {
        JPH::RefConst<JPH::PhysicsMaterial> material = new ColliderMaterial(g.collider->friction, g.collider->restitution);
        const float density = std::max(g.collider->density, 1e-3f);
        switch (g.shape)
        {
        case ColliderShape::Box:
        {
            const JPH::Vec3 half = ToJolt(g.halfExtents);
            JPH::BoxShapeSettings settings(half, std::min(JPH::cDefaultConvexRadius, 0.5f * half.ReduceMin()), material);
            settings.SetEmbedded();
            settings.SetDensity(density);
            return Created(settings.Create());
        }
        case ColliderShape::Sphere:
        {
            JPH::SphereShapeSettings settings(g.radius, material);
            settings.SetEmbedded();
            settings.SetDensity(density);
            return Created(settings.Create());
        }
        case ColliderShape::Capsule:
        {
            if (g.halfHeight <= 1e-4f)
            {
                JPH::SphereShapeSettings settings(g.radius, material);
                settings.SetEmbedded();
                settings.SetDensity(density);
                return Created(settings.Create());
            }
            JPH::CapsuleShapeSettings settings(g.halfHeight, g.radius, material);
            settings.SetEmbedded();
            settings.SetDensity(density);
            return Created(settings.Create());
        }
        case ColliderShape::Cylinder:
        {
            JPH::CylinderShapeSettings settings(g.halfHeight, g.radius, std::min(JPH::cDefaultConvexRadius, 0.5f * std::min(g.halfHeight, g.radius)), material);
            settings.SetEmbedded();
            settings.SetDensity(density);
            return Created(settings.Create());
        }
        }
        return nullptr;
    }

    // One collider as is, one moved off the body's origin, or several as a compound.
    JPH::ShapeRefC MakeShape(const std::vector<ColliderGeometry>& parts)
    {
        if (parts.size() == 1)
        {
            JPH::ShapeRefC shape = MakePrimitive(parts[0]);
            const bool offset = glm::length(parts[0].position) > 1e-6f || std::abs(std::abs(parts[0].rotation.w) - 1.0f) > 1e-6f;
            if (!shape || !offset)
                return shape;
            JPH::RotatedTranslatedShapeSettings settings(ToJolt(parts[0].position), ToJolt(parts[0].rotation), shape);
            settings.SetEmbedded();
            return Created(settings.Create());
        }
        JPH::StaticCompoundShapeSettings compound;
        compound.SetEmbedded();
        for (const ColliderGeometry& part : parts)
            if (JPH::ShapeRefC shape = MakePrimitive(part))
                compound.AddShape(ToJolt(part.position), ToJolt(part.rotation), shape);
        return Created(compound.Create());
    }

    JPH::EMotionType ToJolt(MotionType motion)
    {
        return motion == MotionType::Dynamic ? JPH::EMotionType::Dynamic : motion == MotionType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static;
    }
}

struct PhysicsWorld::Impl
{
    // A body to have: an entity with a RigidBody (its colliders and its children's), or with
    // colliders and no RigidBody above it (a static body of its own colliders).
    struct Candidate
    {
        Entity owner;
        bool hasBody = false;
        RigidBody body;
        glm::mat4 world{ 1.0f };
        std::vector<ColliderGeometry> solid;    // relative to the owner's position and rotation
        std::vector<ColliderGeometry> triggers;
    };

    struct Record
    {
        JPH::BodyID body;   // the solid colliders (or the triggers when there are only those)
        JPH::BodyID sensor; // the triggers of a body that has solid colliders too, following it
        uint64_t signature = 0;
        MotionType motion = MotionType::Static;
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        uint32_t writtenVersion = ~0u; // the owner's LocalTRS.version after the body last moved it
        bool seen = false;
    };

    Scene& scene;
    PhysicsSettings settings;
    BroadPhaseLayers broadPhase;
    ObjectVsBroadPhase objectVsBroadPhase;
    ObjectPairs objectPairs;
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::JobSystem> jobs;
    std::unique_ptr<JPH::PhysicsSystem> system;
    float accumulator = 0.0f;
    std::unordered_map<uint64_t, Record> records;
    std::vector<Candidate> candidates;

    Impl(Scene& scene, const PhysicsSettings& initial) : scene(scene), settings(initial)
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

    ~Impl()
    {
        RemoveAll();
        system.reset();
        jobs.reset();
        temp.reset();
        ReleaseJolt();
    }

    // A new system each time the bodies are all dropped: one with no history (its broad phase,
    // body ids, caches) simulates the same scene the same way every time.
    void CreateSystem()
    {
        system = std::make_unique<JPH::PhysicsSystem>();
        system->Init(65536, 0, 65536, 16384, broadPhase, objectVsBroadPhase, objectPairs);
        system->SetCombineFriction(CombineFriction);
        system->SetCombineRestitution(CombineRestitution);
        Apply();
    }

    void Apply()
    {
        system->SetGravity(ToJolt(settings.gravity));
        objectPairs.Set(settings);
    }

    void RemoveBody(JPH::BodyID& id)
    {
        if (id.IsInvalid())
            return;
        JPH::BodyInterface& bodies = system->GetBodyInterface();
        bodies.RemoveBody(id);
        bodies.DestroyBody(id);
        id = JPH::BodyID();
    }

    void RemoveAll()
    {
        for (auto& [id, record] : records)
        {
            RemoveBody(record.body);
            RemoveBody(record.sensor);
        }
        records.clear();
        accumulator = 0.0f;
    }

    // Every body the scene asks for, from its hierarchy. Shapes are placed relative to their body
    // from the LocalTRS values below it (and the scale above it), so they don't change as it moves.
    void Collect()
    {
        candidates.clear();
        std::function<void(Entity, const glm::mat4&, const glm::vec3&, int, const glm::mat4&)> walk =
            [&](Entity e, const glm::mat4& parentWorld, const glm::vec3& parentScale, int owner, const glm::mat4& parentRelative) {
                const LocalTRS* trs = e.TryGet<LocalTRS>();
                const glm::mat4 local = trs ? LocalMatrix(*trs) : glm::mat4(1.0f);
                const glm::mat4 world = parentWorld * local;
                const glm::vec3 scale = trs ? parentScale * glm::vec3(trs->scl[0], trs->scl[1], trs->scl[2]) : parentScale;
                glm::mat4 relative = parentRelative * local;

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
        // Bodies are made in the order of their entities, not of the scene's storage (which a
        // restored scene may have shuffled).
        std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.owner.RawId() < b.owner.RawId(); });
    }

    JPH::BodyID Create(const Candidate& c, const std::vector<ColliderGeometry>& parts, bool sensor, const glm::vec3& position, const glm::quat& rotation)
    {
        JPH::ShapeRefC shape = MakeShape(parts);
        if (!shape)
            return JPH::BodyID();

        MotionType motion = c.hasBody ? c.body.motion : MotionType::Static;
        uint8_t locked = c.hasBody ? c.body.lockedAxes : 0;
        if (motion == MotionType::Dynamic && (locked & 63) == 63)
            motion = MotionType::Kinematic; // nothing left to move: Jolt can't simulate that
        // The triggers of a body with solid colliders follow it.
        if (sensor && motion == MotionType::Dynamic)
            motion = MotionType::Kinematic;

        JPH::BodyCreationSettings s(shape, ToJolt(position), ToJolt(rotation), ToJolt(motion), MakeLayer(c.hasBody ? c.body.layer : 0, motion != MotionType::Static));
        s.mUserData = c.owner.RawId();
        s.mIsSensor = parts.front().collider->isTrigger;
        s.mCollideKinematicVsNonDynamic = s.mIsSensor; // triggers notice static and kinematic bodies too
        s.mFriction = parts.front().collider->friction;
        s.mRestitution = parts.front().collider->restitution;
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
                s.mLinearVelocity = ToJolt(b.startVelocity);
                s.mAngularVelocity = ToJolt(b.startAngularVelocity);
            }
        }
        return system->GetBodyInterface().CreateAndAddBody(s, motion == MotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    }

    void Sync()
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
                record.seen = true;
                records.emplace(c.owner.RawId(), record);
                ++created;
                continue;
            }

            Record& record = found->second;
            record.seen = true;
            switch (record.motion)
            {
            case MotionType::Static:
                // Moved by the editor or by code.
                if (position != record.position || rotation != record.rotation)
                {
                    bodies.SetPositionAndRotation(record.body, ToJolt(position), ToJolt(rotation), JPH::EActivation::DontActivate);
                    if (!record.sensor.IsInvalid())
                        bodies.SetPositionAndRotation(record.sensor, ToJolt(position), ToJolt(rotation), JPH::EActivation::DontActivate);
                }
                break;
            case MotionType::Kinematic:
                break; // moved toward the entity each step
            case MotionType::Dynamic:
                // Moved by something other than the simulation: put the body there.
                if (trs && trs->version != record.writtenVersion)
                {
                    bodies.SetPositionAndRotation(record.body, ToJolt(position), ToJolt(rotation), JPH::EActivation::Activate);
                    record.writtenVersion = trs->version;
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
            RemoveBody(it->second.body);
            RemoveBody(it->second.sensor);
            it = records.erase(it);
        }

        if (created > 16)
            system->OptimizeBroadPhase();
    }

    void MoveKinematics(float dt)
    {
        JPH::BodyInterface& bodies = system->GetBodyInterface();
        for (auto& [id, record] : records)
        {
            if (record.motion == MotionType::Kinematic && !record.body.IsInvalid())
                bodies.MoveKinematic(record.body, ToJolt(record.position), ToJolt(record.rotation), dt);
        }
    }

    // Dynamic bodies move their entities (in their parent's space); triggers follow their bodies.
    void WriteBack()
    {
        JPH::BodyInterface& bodies = system->GetBodyInterface();
        for (auto& [id, record] : records)
        {
            if (record.body.IsInvalid())
                continue;
            if (!record.sensor.IsInvalid())
            {
                JPH::RVec3 p;
                JPH::Quat q;
                bodies.GetPositionAndRotation(record.body, p, q);
                bodies.SetPositionAndRotation(record.sensor, p, q, JPH::EActivation::DontActivate);
            }
            if (record.motion != MotionType::Dynamic || !bodies.IsActive(record.body))
                continue;

            Entity entity = scene.FromId(id);
            LocalTRS* trs = entity.IsValid() ? entity.Get<LocalTRS>() : nullptr;
            if (!trs)
                continue;

            JPH::RVec3 p;
            JPH::Quat q;
            bodies.GetPositionAndRotation(record.body, p, q);
            glm::vec3 position = ToGlm(p);
            glm::quat rotation = ToGlm(q);
            record.position = position;
            record.rotation = rotation;

            const Entity parent = scene.Parent(entity);
            if (parent.IsValid())
            {
                // Into the parent's space (its scale left out of the rotation).
                const glm::mat4 parentWorld = WorldMatrix(scene, parent);
                const glm::mat4 local = glm::inverse(parentWorld) * (glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation));
                SplitPose(local, position, rotation);
            }

            const float values[7] = { position.x, position.y, position.z, rotation.x, rotation.y, rotation.z, rotation.w };
            float* targets[7] = { &trs->pos[0], &trs->pos[1], &trs->pos[2], &trs->rot_quat[0], &trs->rot_quat[1], &trs->rot_quat[2], &trs->rot_quat[3] };
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
                ++trs->version;
            record.writtenVersion = trs->version;
        }
    }

    void Step(float dt)
    {
        Sync();
        if (dt <= 0.0f)
            return;

        const float step = std::max(settings.fixedTimestep, 1e-4f);
        accumulator += dt;
        int steps = 0;
        // A small tolerance so a frame of exactly one step (the editor's Step button) runs one.
        while (accumulator + 1e-6f >= step && steps < settings.maxStepsPerFrame)
        {
            MoveKinematics(step);
            system->Update(step, std::max(settings.collisionSteps, 1), temp.get(), jobs.get());
            accumulator -= step;
            ++steps;
        }
        if (steps == settings.maxStepsPerFrame && accumulator >= step)
            accumulator = 0.0f; // too far behind: let the simulation run slow instead of piling up
        accumulator = std::max(accumulator, 0.0f);
        if (steps > 0)
            WriteBack();
    }
};

void PhysicsWorld::RegisterComponents()
{
    ECS::RegisterComponent<RigidBody>("RigidBody");
    ECS::RegisterComponent<BoxCollider>("BoxCollider");
    ECS::RegisterComponent<SphereCollider>("SphereCollider");
    ECS::RegisterComponent<CapsuleCollider>("CapsuleCollider");
    ECS::RegisterComponent<CylinderCollider>("CylinderCollider");
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

void PhysicsWorld::Reset()
{
    m_Impl->RemoveAll();
    m_Impl->CreateSystem();
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
