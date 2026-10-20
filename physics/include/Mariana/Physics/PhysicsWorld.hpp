#pragma once
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Physics/PhysicsSettings.hpp>
#include <Mariana/Physics/Queries.hpp>
#include <Mariana/Physics/Vehicle.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <memory>
#include <vector>

class IInput;

// What the simulation has for an entity's body (PhysicsWorld::GetBodyInfo).
struct BodyInfo
{
    MotionType motion = MotionType::Static;
    bool sleeping = false;
    bool sensor = false;
    float mass = 0.0f; // 0 for static and kinematic bodies
    glm::vec3 linearVelocity{ 0.0f };
    glm::vec3 angularVelocity{ 0.0f };
};

// Two bodies that started or stopped touching during the last Step (PhysicsWorld::Events): once per
// pair of bodies, however many of their colliders touch. A trigger reports what enters and leaves
// it. Bodies that fall asleep while touching stay touching.
struct ContactEvent
{
    enum class Type : uint8_t { Begin, End };
    Type type = Type::Begin;
    Entity a, b;             // `a` is the trigger when there is one
    bool trigger = false;
    // Begin only: where they first touched (world space), the normal from `a` toward `b`, and how
    // fast they came together along it (m/s).
    glm::vec3 point{ 0.0f };
    glm::vec3 normal{ 0.0f };
    float speed = 0.0f;
};

// What PhysicsWorld::DebugDraw draws: Jolt's own picture of the bodies, so also what has no editor
// outline (convex hulls, triangle meshes).
struct PhysicsDebugDraw
{
    bool shapes = true;          // every body's shape as lines
    bool sleepColors = false;    // colour shapes by awake (yellow) / asleep (red) instead of by motion type
    bool boundingBoxes = false;
    bool centerOfMass = false;
    bool velocities = false;
    bool contacts = false;       // contact points and normals, from the steps after this was asked for
    glm::vec3 cameraPosition{ 0.0f }; // picks the detail of round shapes
    size_t maxLines = 200000;    // drawing stops there (a level's mesh can have many triangles)
};

struct DebugLine
{
    glm::vec3 from, to;
    uint32_t color; // RGBA8, red in the low byte
};

// The simulation of a scene's RigidBody and collider components, on Jolt Physics (multithreaded
// natively, single-threaded on the web). Step it with game time: bodies are made from the
// components as it goes, made again when what they came from changes (so editing components while
// it runs works), removed when their entity or components go, and dynamic bodies move their
// entities' LocalTRS. Bodies belong to the world, not to the components, so copied entities (prefab
// instances, scene snapshots) get bodies of their own.
class PhysicsWorld
{
public:
    explicit PhysicsWorld(Scene& scene, const PhysicsSettings& settings = {});
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // RigidBody, the colliders, Vehicle, VehicleWheel, PlayerControl and FollowCamera; call before
    // Scene::UpdateComponentRegistry.
    static void RegisterComponents();

    const PhysicsSettings& Settings() const;
    void SetSettings(const PhysicsSettings& settings);

    // Makes the bodies match the components, then runs the fixed steps `dt` seconds of game time
    // cover (time left over carries to the next call). 0 only does the former.
    void Step(float dt);
    // Drops every body; the next Step makes them again from the entities as they are then, and the
    // same scene then simulates the same way every time.
    void Reset();

    size_t BodyCount() const;
    size_t ActiveBodyCount() const;
    // False when the entity has no body (yet).
    bool GetBodyInfo(Entity entity, BodyInfo& info) const;

    // Queries, against the bodies as the last Step left them.
    // The first body along a ray (`direction` needn't be normalized); false when nothing is hit.
    bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& hit, const QueryFilter& filter = {}) const;
    // Every body along a ray, nearest first, once each.
    std::vector<RaycastHit> RaycastAll(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const QueryFilter& filter = {}) const;
    // Sweeps a shape (turned by `rotation`) from `origin`: the first body it would touch. Bodies it
    // already overlaps at the start are hit at distance 0.
    bool ShapeCast(const QueryShape& shape, const glm::vec3& origin, const glm::quat& rotation, const glm::vec3& direction, float maxDistance, RaycastHit& hit, const QueryFilter& filter = {}) const;
    // The bodies a shape at `position` overlaps, each once.
    std::vector<Entity> Overlap(const QueryShape& shape, const glm::vec3& position, const glm::quat& rotation, const QueryFilter& filter = {}) const;

    // Pushing an entity's dynamic body (false when it has no RigidBody that is dynamic). Forces and
    // torques act through every fixed step of the next Step call (call them each frame for a steady
    // push); impulses change the velocity once. Points are in world space.
    bool AddForce(Entity entity, const glm::vec3& force);
    bool AddForceAtPoint(Entity entity, const glm::vec3& force, const glm::vec3& point);
    bool AddTorque(Entity entity, const glm::vec3& torque);
    bool AddImpulse(Entity entity, const glm::vec3& impulse);
    bool AddImpulseAtPoint(Entity entity, const glm::vec3& impulse, const glm::vec3& point);
    bool AddAngularImpulse(Entity entity, const glm::vec3& impulse);
    // Velocities of a dynamic or kinematic body (false when it has no body yet).
    bool SetVelocity(Entity entity, const glm::vec3& linear, const glm::vec3& angular);
    bool GetVelocity(Entity entity, glm::vec3& linear, glm::vec3& angular) const;
    // Puts the entity and its body at a world position and rotation now (queries see it at once).
    bool Teleport(Entity entity, const glm::vec3& position, const glm::quat& rotation);
    bool WakeUp(Entity entity);

    // Contacts that began or ended during the last Step call.
    const std::vector<ContactEvent>& Events() const;
    bool IsTouching(Entity a, Entity b) const;

    // A vehicle's state while it runs (false when the entity has none).
    bool GetVehicleInfo(Entity entity, VehicleInfo& info) const;
    // Sets the Vehicle inputs of every entity with a PlayerControl from the keyboard (call each frame
    // before Step, with the game time, while the player may drive).
    void UpdatePlayerControls(const IInput& input, float dt);

    // Lines picturing the simulation, added to `lines`.
    void DebugDraw(const PhysicsDebugDraw& settings, std::vector<DebugLine>& lines);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
