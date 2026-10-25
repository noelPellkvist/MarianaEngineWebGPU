#pragma once
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

enum class JointType : uint8_t
{
    Fixed,      // welded together
    Point,      // a ball joint: turns any way about the anchor
    Distance,   // keeps the anchors within a distance (a rope or a spring)
    Hinge,      // turns about the joint's X axis (a door, a wheel)
    Slider,     // moves along the joint's X axis (a piston, a drawer)
    Cone,       // a ball joint whose X axis stays within a cone
    SwingTwist, // a shoulder: swings within a cone and twists about X within limits
    SixDOF,     // holds whichever of the six axes are locked, frees the others
};

enum class JointMotor : uint8_t { Off, Velocity, Position };

// Holds two bodies together: the one at or above the joint's entity (the nearest entity with a
// RigidBody, itself or an ancestor) and the connected one (the entity named `connected`, or the body
// at or above it; empty = the world; with `toParent` the body above its own, as in chains and
// ragdolls whose bodies nest). The joint's entity places it: its position is the anchor and
// its axes are the joint's (hinges turn about X, sliders move along X, cones and swing twists twist
// about X, X then Y for the rest), so one body can have several joints on child entities. Made when
// the simulation starts, from the poses then; broken by more force or torque than `breakForce` /
// `breakTorque` (JointInfo::broken), and it stays broken until the simulation is reset (Stop).
struct Joint
{
    JointType type = JointType::Hinge;
    std::string connected;
    bool toParent = false;          // connects to the body above its own (its entity's parent's) instead
    bool collideConnected = false;  // whether the two bodies still collide with each other

    // Hinge (degrees) and slider (m) limits.
    bool limited = false;
    float minLimit = -45.0f;
    float maxLimit = 45.0f;
    // Distance: the other end is `connectedAnchor` on the connected entity (its own space), or a
    // world point when it's the world. Distances below 0 take the one at the start.
    glm::vec3 connectedAnchor{ 0.0f };
    float minDistance = -1.0f;
    float maxDistance = -1.0f;
    // Distance and limits: a spring (Hz; 0 = hard) and its damping ratio.
    float springFrequency = 0.0f;
    float springDamping = 0.5f;
    // Cone and swing twist (degrees): the swing's half angle, and the twist's range.
    float coneAngle = 45.0f;
    float twistMin = -45.0f;
    float twistMax = 45.0f;
    // Six DOF: the axes held (LockedAxis bits, in the joint's axes).
    uint8_t lockedAxes = 0x3F;

    // Hinge and slider: a motor driving it to a speed (deg/s or m/s) or a target (deg or m), with
    // at most `motorStrength` (N m or N); friction when it isn't driven (N m or N).
    JointMotor motor = JointMotor::Off;
    float motorSpeed = 90.0f;
    float motorTarget = 0.0f;
    float motorStrength = 1000.0f;
    float motorFrequency = 2.0f; // position motor: how fast it gets there (Hz)
    float friction = 0.0f;

    float breakForce = 0.0f;     // N; 0 = it never breaks
    float breakTorque = 0.0f;    // N m
};

// What PhysicsWorld::GetJointInfo tells of a joint while the simulation runs.
struct JointInfo
{
    bool active = false;   // made and holding
    bool broken = false;
    float force = 0.0f;    // N it held with in the last step
    float torque = 0.0f;   // N m
    float position = 0.0f; // hinge: angle (deg), slider: position (m), distance: length (m)
};
