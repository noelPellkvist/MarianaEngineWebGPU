#pragma once
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Physics/PhysicsSettings.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <memory>

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

    // RigidBody and the colliders; call before Scene::UpdateComponentRegistry.
    static void RegisterComponents();

    const PhysicsSettings& Settings() const;
    void SetSettings(const PhysicsSettings& settings);

    // Makes the bodies match the components, then runs the fixed steps `dt` seconds of game time
    // cover (time left over carries to the next call). 0 only does the former.
    void Step(float dt);
    // Drops every body; the next Step makes them again from the entities as they are then.
    void Reset();

    size_t BodyCount() const;
    size_t ActiveBodyCount() const;
    // False when the entity has no body (yet).
    bool GetBodyInfo(Entity entity, BodyInfo& info) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
