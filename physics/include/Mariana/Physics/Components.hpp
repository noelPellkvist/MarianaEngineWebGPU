#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Mariana/Assets/AssetManager.hpp>

class IMesh;

enum class MotionType : uint8_t { Static, Kinematic, Dynamic };

// Axes a dynamic body can't move along or turn about (RigidBody::lockedAxes), in world space.
enum LockedAxis : uint8_t
{
    LockPositionX = 1, LockPositionY = 2, LockPositionZ = 4,
    LockRotationX = 8, LockRotationY = 16, LockRotationZ = 32,
};

// Makes an entity a simulated body. Its colliders, and those of its children down to the next
// RigidBody, form the body's shape. Entities with colliders and no RigidBody on or above them are
// static bodies. Dynamic bodies move their entity; kinematic ones follow it (pushing what's in the
// way); static ones stay where it is.
struct RigidBody
{
    MotionType motion = MotionType::Dynamic;
    float mass = 1.0f;            // kg; 0 = the colliders' volume times their density
    float linearDamping = 0.05f;
    float angularDamping = 0.05f;
    float gravityFactor = 1.0f;
    bool continuous = false;      // sweeps fast bodies so they don't pass through thin ones
    bool allowSleeping = true;
    uint8_t lockedAxes = 0;       // LockedAxis bits
    uint32_t layer = 0;           // index into PhysicsSettings::layers
    glm::vec3 startVelocity{ 0.0f };        // world space m/s, given when the body is made
    glm::vec3 startAngularVelocity{ 0.0f }; // world space rad/s
};

// What every collider has: where it sits on its entity (scaled with it) and how its surface acts.
struct Collider
{
    glm::vec3 center{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    float friction = 0.5f;
    float restitution = 0.0f;  // bounciness: 0 = none, 1 = keeps all its speed
    float density = 1000.0f;   // kg/m^3, for bodies whose mass is 0
    bool isTrigger = false;    // reports what enters it instead of colliding
};

struct BoxCollider : Collider
{
    glm::vec3 size{ 1.0f };
};

struct SphereCollider : Collider
{
    float radius = 0.5f;
};

// Along its local Y; `height` is from tip to tip.
struct CapsuleCollider : Collider
{
    float radius = 0.5f;
    float height = 2.0f;
};

// Along its local Y.
struct CylinderCollider : Collider
{
    float radius = 0.5f;
    float height = 1.0f;
};

// A mesh's triangles as a collider (`mesh`, or the entity's MeshComponent mesh when that is empty),
// or with `convex` their convex hull. Triangle meshes are for what doesn't move on its own (levels,
// static and kinematic bodies); on a dynamic body they are used as convex hulls. Scaled with the
// entity, in the mesh's own axes.
struct MeshCollider : Collider
{
    AssetHandle<IMesh> mesh;
    bool convex = false;
};
