#pragma once
#include <Mariana/Scene/ECS.hpp>

#include <string>
#include <glm/glm.hpp>

// Drives the entity's Vehicle or CharacterController from the keyboard
// (PhysicsWorld::UpdatePlayerControls). A vehicle: W / Up throttle, S / Down brakes and then backs
// up, A D / Left Right steer, Space the hand brake. A character: W A S D / arrows walk (relative to
// the view), Shift runs, Space jumps.
struct PlayerControl
{
    bool enabled = true;
    float steerSpeed = 3.0f;   // how fast the steering follows the keys (full lock per 1/speed s)
    float steerFade = 0.0f;    // m/s at which the keys give half the steering lock (less as it goes faster); 0 = full lock at any speed
};

// Puts its entity (a camera) behind and above a target, following it smoothly in game time
// (PhysicsWorld::UpdateFollowCameras): the entity named `target`, or the first one with a
// PlayerControl.
struct FollowCamera
{
    std::string target;
    float distance = 7.0f;     // behind the target (m)
    float height = 2.5f;       // above it
    float lookHeight = 1.0f;   // looks at this far above the target
    float stiffness = 5.0f;    // how quickly it catches up (1/s)
};

// The chase camera's motion, for whatever camera an app has: Update moves `position` toward where a
// FollowCamera would be behind `target` (a world transform; it looks along its +Z) and gives where to
// look. The first call jumps there.
struct ChaseCamera
{
    glm::vec3 position{ 0.0f };
    glm::vec3 lookAt{ 0.0f };
    bool placed = false;

    void Update(const glm::mat4& target, const FollowCamera& settings, float dt);
};
