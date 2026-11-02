#pragma once
// Private to MarianaPhysics: what a PhysicsWorld holds (PhysicsWorld.cpp runs it, each feature's
// file its part; Queries.cpp and DebugDraw.cpp read it).
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Scene/Transform.hpp>
#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Physics/Vehicle.hpp>
#include <Mariana/Physics/Character.hpp>
#include <Mariana/Physics/Joint.hpp>
#include <Mariana/Physics/Water.hpp>
#include <Mariana/Physics/Ragdoll.hpp>
#include <Mariana/Physics/SoftBody.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Geometry/Vertex.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>

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
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Skeleton/SkeletonPose.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
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

    // (An entity's world transform comes from the LocalTRS chain: WorldMatrix, Mariana/Scene/Transform.hpp;
    // WorldXform may not be updated yet for entities made this frame.)
    // Translation and rotation of a transform, without its scale.
    void SplitPose(const glm::mat4& m, glm::vec3& position, glm::quat& rotation);
    // Puts the entity at a world position and rotation (in its parent's space, its scale kept);
    // bumps LocalTRS.version only when something changed.
    void SetWorldPose(const Scene& scene, Entity entity, LocalTRS& trs, glm::vec3 position, glm::quat rotation);

    // The physics components' serializers for scene files (Serializers.cpp).
    void RegisterSerializers();

    // Runtime components (ECS::RegisterRuntimeComponent: never cloned, snapshotted or saved) the world
    // puts on the entities it moves. Its systems write the scene from them; the world keeps what
    // makes them move (Jolt's bodies, characters, constraints), so it can take that away when an
    // entity or its components go.

    // An entity the simulation moves (a dynamic body, a character, a ragdoll's bone): where the last
    // two steps left it, which the write-back system shows `alpha` of the way between.
    struct PhysicsBody
    {
        uint32_t body = JPH::BodyID::cInvalidBodyID; // the body it follows (index and sequence); invalid: its owner tracks it (a character)
        bool interpolate = true;
        bool show = true;              // false while the simulation doesn't move it (an Animated ragdoll's bone)
        uint32_t writtenVersion = ~0u; // its LocalTRS.version after the write-back last moved it
        glm::vec3 previousPosition{ 0.0f }, currentPosition{ 0.0f }, shownPosition{ 0.0f };
        glm::quat previousRotation{ 1.0f, 0.0f, 0.0f, 0.0f }, currentRotation{ 1.0f, 0.0f, 0.0f, 0.0f }, shownRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        void Place(const glm::vec3& p, const glm::quat& r)
        {
            previousPosition = currentPosition = shownPosition = p;
            previousRotation = currentRotation = shownRotation = r;
        }
    };

    // A vehicle's wheel entity: which wheel of which chassis it shows.
    struct PhysicsWheel
    {
        uint64_t chassis = 0;
        uint32_t index = 0;
    };

    // A soft body's entity: how the copy of its mesh it draws (its MeshOverride, written every frame
    // it moves) is made from the particles, and where the particles were after the last two steps.
    struct PhysicsSoftBody
    {
        uint32_t body = JPH::BodyID::cInvalidBodyID;
        std::vector<StandardVertex> vertices;  // the copy's
        std::vector<uint32_t> triangles;       // the copy's triangles (its indices), for tangents
        std::vector<uint32_t> particleOf;      // per vertex (~0u: not part of a triangle)
        std::vector<uint32_t> faces;           // particles, three per face, wound as modelled
        std::vector<uint32_t> normalStart, normalFaces; // per vertex: the faces its normal is made of
        std::vector<glm::vec3> previous, current; // particles, world
        std::vector<glm::vec3> shown, faceNormals; // scratch
        bool awake = false, written = false;
    };

    // FNV-1a over what something is made from: when its signature changes, it is made again.
    struct Signature
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
        void Add(const std::string& s) { Bytes(s.data(), s.size()); }
    };

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

    // A body the world owns (a dynamic one's entity also has a PhysicsBody, for the write-back).
    struct Record
    {
        JPH::BodyID body;   // the solid colliders (or the triggers when there are only those)
        JPH::BodyID sensor; // the triggers of a body that has solid colliders too, following it
        uint64_t signature = 0;
        MotionType motion = MotionType::Static;
        glm::vec3 position{ 0.0f }; // where its entity was at the last sync (kinematic bodies go there)
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        bool seen = false;
    };

    // Where an entity is during a sync: its world matrix and scale from the root down (as the
    // transform system multiplies them), and whether it is on or below a vehicle's wheel or a
    // character (no body is made of what is there).
    struct Node
    {
        glm::mat4 world{ 1.0f };
        glm::vec3 scale{ 1.0f };
        bool excluded = false;
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
        std::unique_ptr<JPH::BodyFilter> wheelFilter; // what its wheels stand on (outlives the constraint)
        JPH::Ref<JPH::VehicleConstraint> constraint;
        JPH::BodyID body;
        VehicleType type = VehicleType::Wheeled;
        uint64_t signature = 0;
        std::vector<Wheel> wheels;
        bool seen = false;
    };

    // A character (its entity also has a PhysicsBody, which TrackPoses fills from it).
    struct CharacterRecord
    {
        JPH::Ref<JPH::CharacterVirtual> character;
        JPH::BodyID innerBody;
        uint64_t signature = 0;
        uint64_t animator = 0; // the Animator walking it (root motion, on it or below it); 0 = none
        float yaw = 0.0f;
        bool seen = false;
        bool tracked = false; // stepped since it was made or put somewhere
    };

    struct JointRecord
    {
        JPH::Ref<JPH::TwoBodyConstraint> constraint;
        JPH::BodyID body1, body2; // body2 invalid: the world
        uint64_t signature = 0;
        uint64_t noCollidePair = 0;
        bool hasNoCollidePair = false;
        float force = 0.0f, torque = 0.0f;
        bool broken = false; // pulled apart: not made again until Reset
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

    // A WaterVolume as placed this step: its box (center, axes, half extents), its still surface's
    // center and its world bounds.
    struct WaterRecord
    {
        uint64_t entity = 0;
        WaterVolume water;
        glm::vec3 center{ 0.0f }, top{ 0.0f }, half{ 0.0f };
        glm::vec3 right{ 1.0f, 0.0f, 0.0f }, up{ 0.0f, 1.0f, 0.0f }, forward{ 0.0f, 0.0f, 1.0f };
        glm::vec3 boundsMin{ 0.0f }, boundsMax{ 0.0f };
    };

    // A ragdoll on a model's root: Jolt's ragdoll (bodies in PlanRagdoll's order, parents first),
    // each bone's entity (which has a PhysicsBody) and capsule, and how it moves now.
    struct RagdollRecord
    {
        struct Bone
        {
            uint64_t entity = 0;
            JPH::BodyID body;
            float radius = 0.0f;
            glm::vec3 localA{ 0.0f }, localB{ 0.0f }; // the capsule's ends in the body's frame
            LocalTRS rest{};                          // its transform when made (a pose without an Animator)
        };
        uint64_t root = 0;
        JPH::Ref<JPH::RagdollSettings> settings;
        JPH::Ref<JPH::Ragdoll> ragdoll;
        std::vector<Bone> bones;
        uint64_t signature = 0;
        RagdollMode mode = RagdollMode::Powered; // as applied to the bodies and motors
        float strength = 0.0f;
        bool knockedOut = false; // hit too hard: limp until the mode is set again
        RagdollMode asked = RagdollMode::Powered; // the component's mode last step (a change gets it up)
        JPH::SkeletonPose previousTarget; // last step's animated pose, so the motors follow its motion too
        bool hasPreviousTarget = false;
        bool seen = false;
    };

    // A soft body on an entity: Jolt's body (what the entity draws now, made from the particles, is
    // its MeshOverride and PhysicsSoftBody).
    struct SoftBodyRecord
    {
        JPH::BodyID body;
        uint64_t signature = 0;
        bool made = false;
        std::vector<uint32_t> faces;           // particles, three per face, wound as modelled (the wind's)
        std::vector<std::pair<uint32_t, glm::vec3>> pins; // particle, its place in the entity's space
        float invMass = 1.0f;
        float restVolume = 0.0f;
        bool closed = false;
        float pressureScale = 0.0f; // Jolt's pressure (n R T) for a push of one weight
        bool seen = false;
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
    // Pushes made before a fixed step (beforeStep), for that step only.
    std::unordered_map<uint64_t, Pending> stepPending;
    std::function<void(float)> beforeStep;
    bool inBeforeStep = false;
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
    std::vector<WaterRecord> waters; // found by Collect
    std::unordered_map<uint64_t, RagdollRecord> ragdolls; // per model root
    std::unordered_map<uint64_t, uint64_t> ragdollBones;  // bone entity -> its ragdoll's root
    uint32_t ragdollGroups = 0;
    std::unordered_map<uint64_t, SoftBodyRecord> softBodies; // per entity
    // The materials the water scrolls, and their normal maps' offset before (Reset puts it back).
    std::vector<std::pair<AssetHandle<MaterialAsset>, glm::vec2>> rippled;
    std::unordered_map<uint64_t, Node> nodes; // the entities placed during this sync
    // The systems that follow the bodies after each step (TrackPoses) and write the scene (Show).
    System trackBodies, trackSoftBodies;
    System showBodies, showWheels, showSoftBodies;
    float waterTime = 0.0f;          // simulated seconds, for the waves
    // Debug drawing of contacts: wanted by the last DebugDraw call, drawn by Jolt during steps.
    bool drawContacts = false;
    size_t maxContactLines = 0;
    std::vector<DebugLine> contactLines;

    Impl(Scene& scene, const PhysicsSettings& initial);
    ~Impl();

    void CreateSystem();
    void MakeSystems();
    void Apply();
    void RemoveBody(JPH::BodyID& id);
    void RemoveAll();
    // The active entities with any of the components (Scene::Each), each once, in id order: bodies are
    // made in this order, so the simulation doesn't depend on how the scene stores its entities. An
    // inactive entity (or one below an inactive one) has no body, joint, vehicle or character.
    template <class... T>
    std::vector<Entity> Find() const
    {
        std::vector<Entity> found;
        (scene.Each<T>([&](Entity e, const T&) {
             if (e.IsActive())
                 found.push_back(e);
         }),
         ...);
        std::sort(found.begin(), found.end(), [](Entity a, Entity b) { return a.RawId() < b.RawId(); });
        found.erase(std::unique(found.begin(), found.end(), [](Entity a, Entity b) { return a.RawId() == b.RawId(); }), found.end());
        return found;
    }
    // Entities by name, for what refers to one by name (a joint's `connected`, a camera's target):
    // as Scene::FindByName, the one with the smallest id when several share it.
    std::unordered_map<std::string, Entity> EntitiesByName() const;
    const Node& NodeOf(Entity entity);
    void Collect();
    JPH::BodyID Create(const Candidate& c, std::vector<ColliderGeometry> parts, bool sensor, const glm::vec3& position, const glm::quat& rotation);
    void Sync();
    void MoveKinematics(float dt);
    void ApplyPending(const std::unordered_map<uint64_t, Pending>& pushes, bool impulses);
    void SyncVehicles();
    void RemoveVehicle(VehicleRecord& record);
    void DriveVehicles();
    // The wheel system's: a wheel entity where Jolt has its wheel, on its chassis as shown.
    void ShowWheel(Entity entity, const PhysicsInternal::PhysicsWheel& wheel, LocalTRS& trs);
    void SyncCharacters();
    void RemoveCharacter(uint64_t entity, CharacterRecord& record);
    void MoveCharacters(float dt);
    // After each step: where each character is now, into its entity's PhysicsBody.
    void TrackCharacters();
    // After the steps of a Step call: the jumps asked for are spent, and the root motion walked.
    void SpendJumps();
    // The Animator with root motion on a character's entity or below it (not inside another
    // character), or an invalid entity.
    Entity RootMotionAnimator(Entity character) const;
    void SyncJoints();
    void RemoveJoint(JointRecord& record);
    // Takes away the joints holding a body (before the body goes).
    void DropJointsOf(JPH::BodyID body);
    void DriveJoints();
    void CheckJoints(float dt);
    void ProcessContacts();
    void AddWater(Entity entity, const glm::mat4& world, const WaterVolume& water);
    // The water a point is in (within its sides and depth, waves included), or null.
    const WaterRecord* WaterAt(const glm::vec3& point) const;
    void ApplyBuoyancy(float dt);
    void SyncRagdolls();
    void BuildRagdoll(Entity root, const Ragdoll& settings, RagdollRecord& record);
    void RemoveRagdoll(RagdollRecord& record);
    // The ragdoll's bones where the animation has them (world space).
    bool TargetPose(const RagdollRecord& record, JPH::SkeletonPose& pose);
    void DriveRagdolls(float dt);
    // Contact events from `firstEvent` on that knock a ragdoll out.
    void CheckRagdollImpacts(size_t firstEvent);
    void KnockBack(RagdollRecord& record, const Ragdoll& settings, Entity hitter, const glm::vec3& point);
    // Whether the write-back moves the bones' entities (not while the animation moves the bodies).
    void ShowRagdoll(const RagdollRecord& record, bool show);
    void SyncSoftBodies();
    void BuildSoftBody(Entity entity, const SoftBody& settings, const AssetHandle<IMesh>& source, SoftBodyRecord& record);
    // Takes the body away and gives the entity back the mesh it had.
    void RemoveSoftBody(uint64_t entity, SoftBodyRecord& record);
    // Before each step: pinned particles follow the entity, the wind blows.
    void DriveSoftBodies(float dt);
    // The soft body systems': after each step, where its particles are; and the mesh written `alpha`
    // of the way from the previous step's particles to the last's.
    void TrackSoftBody(PhysicsInternal::PhysicsSoftBody& drawn);
    void ShowSoftBody(Entity entity, PhysicsInternal::PhysicsSoftBody& drawn, IMesh* mesh, float alpha);
    // Scrolls the normal maps of the water's meshes (the surface).
    void RippleWater();
    // After each step: the poses moving bodies, characters and particles reached, for interpolation.
    void TrackPoses();
    // Runs the write-back systems: entities `alpha` of the way from where their previous step left
    // them to where the last did (1 = the last), wheels on their chassis, soft bodies' meshes.
    void Show(float alpha);
    void Step(float dt);

    // The entity's body (its solid one when it has both), invalid when it has none.
    JPH::BodyID BodyOf(Entity entity) const;
    // The entity a body belongs to.
    Entity EntityOf(JPH::BodyID body) const;
    // Queues for the entity's dynamic body; null when it has no RigidBody that would be dynamic.
    Pending* PendingFor(Entity entity);
};
