#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Physics/Vehicle.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Ready-to-drive vehicles and a place to drive them, made of the engine's primitives (so any app can
// spawn them, and they show how to build one).
namespace VehiclePresets
{
    // A car (1.5 t, rear-wheel drive), a motorcycle (240 kg, leaning) or a tank (4 t, tracks with a
    // turret), its chassis at `position` facing +Z turned by `rotation`, with a PlayerControl. Its
    // wheels are VehicleWheel entities with a tyre mesh and a spoke below them (so they visibly spin).
    Entity Spawn(Scene& scene, AssetManager2& assets, VehicleType type, const glm::vec3& position, const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // A 200 m ground with a tabletop jump, a kicker, a bump field, a banked turn and a wall of light
    // boxes to drive through, centred on `center`.
    Entity SpawnPlayground(Scene& scene, AssetManager2& assets, const glm::vec3& center);
}
