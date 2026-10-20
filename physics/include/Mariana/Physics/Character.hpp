#pragma once
#include <Mariana/Scene/ECS.hpp>

#include <cstdint>
#include <glm/glm.hpp>

// A walking character on Jolt's CharacterVirtual: a capsule moved by its own collision tests rather
// than simulated as a body, so it goes where it is steered, walks up steps, stays on slopes it can
// climb and slides off those it can't, rides moving platforms and pushes dynamic bodies (which push
// it back). A kinematic body moves with it so the rest of the world sees it (queries, triggers,
// bodies bumping into it). The entity's position is at the character's feet and it turns about +Y
// only; colliders on or below it take no part in it.
struct CharacterController
{
    float radius = 0.3f;
    float height = 1.8f;        // feet to the top of the head
    float maxSlope = 50.0f;     // degrees it can walk up
    float stepHeight = 0.35f;   // steps it walks up
    float stickDown = 0.5f;     // how far it follows the ground down (steps, slopes) instead of flying off
    float mass = 70.0f;         // kg, for pushing and being pushed
    float strength = 200.0f;    // N it pushes with
    float speed = 4.0f;         // m/s walking
    float runFactor = 1.8f;     // times faster running
    float jumpSpeed = 5.0f;     // m/s up
    float gravityFactor = 1.0f;
    float airControl = 0.3f;    // 0 = keeps its velocity in the air, 1 = steers as on the ground
    bool turnToMove = true;     // turns to face where it walks
    float turnSpeed = 720.0f;   // degrees per second
    uint32_t layer = 0;

    // The driver (code, or PlayerControl): where to walk (world space, horizontal, length up to 1),
    // whether to run, and a jump (taken at the next step on the ground, then cleared).
    glm::vec3 move{ 0.0f };
    bool run = false;
    bool jump = false;
};

// What PhysicsWorld::GetCharacterInfo tells of a character while it runs.
struct CharacterInfo
{
    bool grounded = false;       // standing on something it can walk on
    bool onSteepGround = false;  // touching ground too steep to stand on
    glm::vec3 velocity{ 0.0f };
    glm::vec3 groundNormal{ 0.0f };
    Entity ground;               // what it stands on
};
