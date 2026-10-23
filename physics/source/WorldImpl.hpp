#pragma once
// Private to MarianaPhysics: what a PhysicsWorld holds (PhysicsWorld.cpp runs it; Queries.cpp and
// DebugDraw.cpp read it).
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Physics/Vehicle.hpp>
#include <Mariana/Physics/Character.hpp>
#include <Mariana/Physics/Joint.hpp>

#include "JoltCommon.hpp"
#include "Shapes.hpp"

#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PhysicsInternal
{
    // Contacts as Jolt reports them during a step (from its worker threads natively).
    class ContactCollector final : public JPH::ContactListener
    {
    public:
        struct Added
        {
            JPH::BodyID body1, body2;     // as Jolt orders them (smaller id first)
            uint64_t subShapes = 0;       // SubShapePairKey of the two sub-shapes
            uint64_t entityA = 0, entityB = 0; // A is the trigger when there is one
            bool trigger = false;
            glm::vec3 point{ 0.0f }, normal{ 0.0f }; // normal from A toward B
            float speed = 0.0f;
        };

        struct Removed
        {
            JPH::BodyID body1, body2;
            uint64_t subShapes = 0;
        };

        void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override;
        void OnContactRemoved(const JPH::SubShapeIDPair& pair) override;
        // Characters' own contacts (with triggers), reported on the main thread.
        void Add(const Added& contact);
        void Remove(const Removed& contact);

        // What was collected since the last call, in an order that doesn't depend on threads.
        void Take(std::vector<Added>& added, std::vector<Removed>& removed);
        void Clear();

    private:
        std::mutex m_Mutex;
        std::vector<Added> m_Added;
        std::vector<Removed> m_Removed;
    };

    // Characters report what they touch through this (a trigger they walk into, say).
    class CharacterContacts final : public JPH::CharacterContactListener
    {
    public:
        ContactCollector* collector = nullptr;
        void OnContactAdded(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings) override;
        void OnContactRemoved(const JPH::CharacterVirtual* character, const JPH::BodyID& body, const JPH::SubShapeID& subShape) override;
    };

    // Bodies a joint holds that mustn't collide with each other: their collision groups carry their
    // body index, and the pairs are listed here (read by Jolt's threads, changed only between steps).
    class JointCollisionFilter final : public JPH::GroupFilter
    {
    public:
        std::unordered_set<uint64_t> pairs;
        static uint64_t PairKey(uint32_t a, uint32_t b)
        {
            if (a > b)
                std::swap(a, b);
            return (static_cast<uint64_t>(a) << 32) | b;
        }
        bool CanCollide(const JPH::CollisionGroup& a, const JPH::CollisionGroup& b) const override
        {
            return pairs.find(PairKey(a.GetGroupID(), b.GetGroupID())) == pairs.end();
        }
    };

    // A character's contact as a sub-shape pair key no body contact has.
    inline uint64_t CharacterContactKey(JPH::SubShapeID subShape)
    {
        return (0xC7A2ull << 48) | subShape.GetValue();
    }

    inline uint64_t SubShapePairKey(JPH::SubShapeID a, JPH::SubShapeID b)
    {
        return (static_cast<uint64_t>(a.GetValue()) << 32) | b.GetValue();
    }

    // Two bodies, smaller id first.
    inline uint64_t BodyPairKey(JPH::BodyID a, JPH::BodyID b)
    {
        uint32_t x = a.GetIndexAndSequenceNumber(), y = b.GetIndexAndSequenceNumber();
        if (x > y)
            std::swap(x, y);
        return (static_cast<uint64_t>(x) << 32) | y;
    }

    glm::mat4 LocalMatrix(const LocalTRS& t);
    // The entity's world transform from the LocalTRS chain (WorldXform may not be updated yet for
    // entities made this frame).
    glm::mat4 WorldMatrix(const Scene& scene, Entity entity);
    // Translation and rotation of a transform, without its scale.
    void SplitPose(const glm::mat4& m, glm::vec3& position, glm::quat& rotation);
    // Puts the entity at a world position and rotation (in its parent's space, its scale kept);
    // bumps LocalTRS.version only when something changed.
    void SetWorldPose(const Scene& scene, Entity entity, LocalTRS& trs, glm::vec3 position, glm::quat rotation);

    // Jolt's debug renderer (one for the process), collecting lines while a target is set: the
    // contacts Jolt draws during steps, and DebugDraw's bodies.
    void AcquireDebugRenderer();
    void ReleaseDebugRenderer();
    void BeginStepDrawing(std::vector<DebugLine>* lines, size_t maxLines);
    void EndStepDrawing();
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
        // Dynamic bodies: where the last two steps left them, and where the entity is shown (between
        // the two with interpolation).
        bool interpolate = true;
        glm::vec3 previousPosition{ 0.0f }, currentPosition{ 0.0f }, shownPosition{ 0.0f };
        glm::quat previousRotation{ 1.0f, 0.0f, 0.0f, 0.0f }, currentRotation{ 1.0f, 0.0f, 0.0f, 0.0f }, shownRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        void Place(const glm::vec3& p, const glm::quat& r)
        {
            previousPosition = currentPosition = shownPosition = p;
            previousRotation = currentRotation = shownRotation = r;
        }
    };

    // Forces and impulses waiting for the next Step (AddForce and the rest).
    struct Pending
    {
        glm::vec3 force{ 0.0f }, torque{ 0.0f }, impulse{ 0.0f }, angularImpulse{ 0.0f };
        std::vector<std::pair<glm::vec3, glm::vec3>> pointForces, pointImpulses; // (amount, world point)
    };

    // Two bodies in contact: the sub-shape pairs touching, and those Jolt stopped reporting because
    // a body fell asleep (still touching; dropped when it wakes up and the contact isn't found again).
    // A vehicle constraint on a chassis body, and the wheels it moves.
    struct VehicleRecord
    {
        struct Wheel
        {
            uint64_t entity = 0;
            JPH::Vec3 modelRight = JPH::Vec3::sAxisX(), modelUp = JPH::Vec3::sAxisY(); // the wheel entity's axes that map to the axle and up
        };
        JPH::Ref<JPH::VehicleConstraint> constraint;
        JPH::BodyID body;
        VehicleType type = VehicleType::Wheeled;
        uint64_t signature = 0;
        std::vector<Wheel> wheels;
        bool seen = false;
    };

    struct CharacterRecord
    {
        JPH::Ref<JPH::CharacterVirtual> character;
        JPH::BodyID innerBody;
        uint64_t signature = 0;
        uint32_t writtenVersion = ~0u; // the entity's LocalTRS.version after the character last moved it
        float yaw = 0.0f;
        bool seen = false;
        // Where the last two steps left it (feet and yaw), for interpolation.
        glm::vec3 previousPosition{ 0.0f }, currentPosition{ 0.0f };
        float previousYaw = 0.0f, currentYaw = 0.0f;
        bool tracked = false;
    };

    struct JointRecord
    {
        JPH::Ref<JPH::TwoBodyConstraint> constraint;
        JPH::BodyID body1, body2; // body2 invalid: the world
        uint64_t signature = 0;
        uint64_t noCollidePair = 0;
        bool hasNoCollidePair = false;
        float force = 0.0f, torque = 0.0f;
        bool seen = false;
    };

    // Where a wheel entity sits when its suspension is fully out: its LocalTRS before the vehicle
    // moved it (and the version it left), so its own moves are told apart from the edits of others.
    struct WheelRest
    {
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        uint32_t writtenVersion = ~0u;
    };

    struct Touching
    {
        JPH::BodyID body1, body2;
        uint64_t a = 0, b = 0; // entities, a the trigger when there is one
        bool trigger = false;
        std::unordered_set<uint64_t> live;
        std::unordered_map<uint64_t, int> asleep; // sub-shape pair -> steps since both woke
        bool Empty() const { return live.empty() && asleep.empty(); }
    };

    Scene& scene;
    PhysicsSettings settings;
    PhysicsInternal::BroadPhaseLayers broadPhase;
    PhysicsInternal::ObjectVsBroadPhase objectVsBroadPhase;
    PhysicsInternal::ObjectPairs objectPairs;
    std::unique_ptr<JPH::TempAllocatorImpl> temp;
    std::unique_ptr<JPH::JobSystem> jobs;
    std::unique_ptr<JPH::PhysicsSystem> system;
    PhysicsInternal::ShapeCache shapes;
    PhysicsInternal::ContactCollector contacts;
    float accumulator = 0.0f;
    std::unordered_map<uint64_t, Record> records;
    std::vector<Candidate> candidates;
    std::unordered_map<uint64_t, Pending> pending;
    // Body id -> entity, also for bodies removed since the last step (Jolt reports their contacts
    // ending in the next one).
    std::unordered_map<uint32_t, uint64_t> bodyEntities;
    std::vector<uint32_t> removedBodies;
    std::unordered_map<uint64_t, Touching> touching; // per BodyPairKey
    std::vector<ContactEvent> events;
    std::unordered_set<uint64_t> warnedDynamicMesh;
    std::unordered_map<uint64_t, VehicleRecord> vehicles; // per chassis entity
    std::unordered_map<uint64_t, WheelRest> wheelRests;   // per wheel entity
    std::unordered_map<uint64_t, CharacterRecord> characters;
    std::unique_ptr<JPH::CharacterVsCharacterCollisionSimple> characterCollision;
    PhysicsInternal::CharacterContacts characterContacts;
    std::unordered_map<uint64_t, JointRecord> joints; // per joint entity
    JPH::Ref<PhysicsInternal::JointCollisionFilter> jointFilter; // made with the first system (Jolt's allocator is set up then)
    std::vector<Entity> brokenJoints;
    // Debug drawing of contacts: wanted by the last DebugDraw call, drawn by Jolt during steps.
    bool drawContacts = false;
    size_t maxContactLines = 0;
    std::vector<DebugLine> contactLines;

    Impl(Scene& scene, const PhysicsSettings& initial);
    ~Impl();

    void CreateSystem();
    void Apply();
    void RemoveBody(JPH::BodyID& id);
    void RemoveAll();
    void Collect();
    JPH::BodyID Create(const Candidate& c, std::vector<ColliderGeometry> parts, bool sensor, const glm::vec3& position, const glm::quat& rotation);
    void Sync();
    void MoveKinematics(float dt);
    void ApplyPending(bool impulses);
    void SyncVehicles();
    void RemoveVehicle(VehicleRecord& record);
    void DriveVehicles();
    void WriteWheels();
    void SyncCharacters();
    void RemoveCharacter(CharacterRecord& record);
    void MoveCharacters(float dt);
    void WriteCharacters(float alpha, bool stepped);
    void SyncJoints();
    void RemoveJoint(JointRecord& record);
    // Takes away the joints holding a body (before the body goes).
    void DropJointsOf(JPH::BodyID body);
    void DriveJoints();
    void CheckJoints(float dt);
    void ProcessContacts();
    // After each step: the poses moving bodies and characters reached, for interpolation.
    void TrackPoses();
    // Shows bodies, wheels and characters `alpha` of the way from their previous step to their last
    // (1 = where the last step left them).
    void WriteBack(float alpha);
    void Step(float dt);

    // The entity's body (its solid one when it has both), invalid when it has none.
    JPH::BodyID BodyOf(Entity entity) const;
    // The entity a body belongs to.
    Entity EntityOf(JPH::BodyID body) const;
    // Queues for the entity's dynamic body; null when it has no RigidBody that would be dynamic.
    Pending* PendingFor(Entity entity);
};
