#pragma once
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>
#include <vector>

// How a ragdoll moves: with the animation (kinematic: it pushes things but nothing moves it),
// powered (its joints' motors hold the animated pose and its hips follow the animation, so it can
// be knocked about), or limp (it just falls).
enum class RagdollMode : uint8_t { Animated, Powered, Limp };

// A ragdoll for an animated model: goes on its root (the entity with the Animator). Made when the
// simulation starts from the model's skeleton and skin: a capsule per main bone (hips, spine, neck,
// head, upper and lower arms, hands, thighs, shins, feet; fingers, toes and end bones are left to
// follow), sized from the skin's vertices around it, joined to the nearest bone above it that has
// a body (knees and elbows by hinges bending their way, the rest by swing twist joints with limits
// by what the bone is called). Bones it moves are written back to their entities (the bones it
// leaves out keep the animation's pose under them).
struct Ragdoll
{
    RagdollMode mode = RagdollMode::Powered;
    float mass = 80.0f;            // kg, shared out by the bones' volume
    float strength = 600.0f;       // N m each joint's motor may use (Powered)
    float limpOnImpact = 4.0f;     // m/s: something hitting it faster makes it go limp; 0 = never
    float friction = 0.6f;
    float jointLimitScale = 1.0f;  // widens (> 1) or narrows every joint's limits
    uint32_t layer = 0;            // PhysicsSettings layer
};

// What joins a ragdoll bone to the one above it.
enum class RagdollJointKind : uint8_t { Root, Spine, Neck, Head, Shoulder, Elbow, Wrist, Hip, Knee, Ankle, Other };

// One body of a ragdoll as it would be made now (PlanRagdoll): its bone's entity, the bone with a
// body above it (-1 for the root), how they are joined, and its capsule in world space (end to end
// along `a`..`b`, radius) in the skin's bind pose.
struct RagdollBone
{
    Entity entity;
    std::string name;
    int parent = -1;
    RagdollJointKind joint = RagdollJointKind::Other;
    glm::vec3 position{ 0.0f };                    // the bone's origin (bind pose, world)
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };  // the bone's axes (bind pose, world)
    glm::vec3 a{ 0.0f }, b{ 0.0f };
    float radius = 0.05f;
};

// The bones a ragdoll on `root` would have. Empty when it has no bones below it.
std::vector<RagdollBone> PlanRagdoll(const Scene& scene, Entity root);

// What PhysicsWorld::GetRagdollInfo tells of a ragdoll while it runs.
struct RagdollInfo
{
    RagdollMode mode = RagdollMode::Powered;
    struct Bone
    {
        Entity entity;
        glm::vec3 position{ 0.0f };   // its body (world)
        glm::vec3 a{ 0.0f }, b{ 0.0f }; // its capsule's ends now (world)
        float radius = 0.0f;
        bool touching = false;        // in contact with something outside the ragdoll
    };
    std::vector<Bone> bones;
};
