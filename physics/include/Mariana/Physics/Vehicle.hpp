#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

// Vehicles on Jolt's vehicle constraint. A Vehicle goes on an entity with a dynamic RigidBody (the
// chassis, whose colliders are the body); its wheels are the entities below it with a VehicleWheel.
// The chassis drives along its +Z with +Y up, so +X is its right. Each wheel entity is placed where
// its wheel hangs with the suspension fully out; while the simulation runs the vehicle moves it as
// the wheel travels, steers and spins (so a wheel mesh on it, or below it, follows). Wheels take no
// part in the chassis's shape (colliders on them are ignored).
enum class VehicleType : uint8_t
{
    Wheeled,    // a car or truck: an engine, a gearbox and differentials to the driven axles
    Motorcycle, // two wheels in line, leaning into turns to stay up
    Tracked,    // a tank: the wheels on each side run in a track, steered by the tracks' speeds
};

// How a wheel finds the ground: a ray (cheapest), a sphere or a cylinder of the wheel's size
// (smoothest over edges and steps).
enum class WheelCollision : uint8_t { Ray, Sphere, Cylinder };

// Which axles the engine drives (wheeled vehicles; axles are the wheels side by side, frontmost
// first).
enum class VehicleDrive : uint8_t { Front, Rear, All };

struct Vehicle
{
    VehicleType type = VehicleType::Wheeled;
    WheelCollision collision = WheelCollision::Cylinder;
    VehicleDrive drive = VehicleDrive::Rear;

    // Engine (Nm, revolutions per minute).
    float engineTorque = 500.0f;
    float minRPM = 1000.0f;
    float maxRPM = 6000.0f;
    // Gearbox: automatic, or the gear in `gear` (1 is first, -1 reverse, 0 neutral).
    bool automatic = true;
    float shiftUpRPM = 4000.0f;
    float shiftDownRPM = 2000.0f;
    float clutchStrength = 10.0f;
    std::vector<float> gearRatios{ 2.66f, 1.78f, 1.3f, 1.0f, 0.74f };
    float reverseRatio = -2.9f;
    float differentialRatio = 3.42f; // gearbox to wheels (wheeled), or to the tracks' driven wheels
    float limitedSlipRatio = 1.4f;   // max / min wheel speed across a differential; 0 = open
    float antiRollStiffness = 1000.0f; // N/m for each axle; 0 = none
    float maxPitchRollAngle = 60.0f;   // degrees it may tip before it is held up
    float maxLeanAngle = 45.0f;        // motorcycles: degrees it leans into turns

    // The driver (set by code, or by PlayerControl): throttle -1..1 (negative backs up),
    // steering -1..1 (positive turns right), brake and hand brake 0..1.
    float throttle = 0.0f;
    float steering = 0.0f;
    float brake = 0.0f;
    float handBrake = 0.0f;
    int gear = 1;
};

struct VehicleWheel
{
    float radius = 0.35f;
    float width = 0.25f;
    // Suspension: travel from the attachment point (fully up to fully out, m), the spring's
    // frequency (Hz) and damping ratio.
    float suspensionMin = 0.1f;
    float suspensionMax = 0.3f;
    float suspensionFrequency = 1.5f;
    float suspensionDamping = 0.5f;
    float maxSteerAngle = 0.0f;   // degrees; 0 = it doesn't steer
    float casterAngle = 0.0f;     // degrees the steering axis leans back (motorcycle forks)
    float maxBrakeTorque = 1500.0f;     // Nm
    float maxHandBrakeTorque = 0.0f;    // Nm
    float grip = 1.0f;                  // scales the tyre's friction
};

// What PhysicsWorld::GetVehicleInfo tells of a vehicle while it runs.
struct VehicleInfo
{
    struct Wheel
    {
        bool contact = false;
        float suspension = 0.0f;     // 0 = fully out, 1 = fully up
        float steerAngle = 0.0f;     // degrees
        float rpm = 0.0f;
        float longitudinalSlip = 0.0f;
        float lateralSlip = 0.0f;    // degrees
    };
    float speed = 0.0f;  // m/s along the chassis's forward
    float rpm = 0.0f;
    int gear = 0;
    std::vector<Wheel> wheels;
};
