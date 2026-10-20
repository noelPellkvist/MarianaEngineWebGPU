#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Physics/Vehicle.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>

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

    // Makes a car model drivable: it faces +Z and stands on its origin (as glTF models do), its
    // wheels are the entities below it named "Wheel..." (not inside another wheel), spinning about
    // their axle (the axis of theirs nearest the car's X; a wheel modelled steered is turned
    // straight) and sized from their meshes, front ones (the frontmost axle) steering. The body
    // collides as the convex hulls of its three biggest meshes. The root gets a RigidBody (`mass`),
    // a Vehicle (rear drive) and a PlayerControl. The wheels are moved down by how far the springs
    // settle and the car up by as much, so it comes to rest as it was modelled. Call after
    // Scene::Update (it reads the world transforms); false (and nothing changed) without at least 3
    // wheels.
    bool RigCar(Scene& scene, Entity model, float mass = 1500.0f);
    // Loads a car model (a resource path), puts it at `position` turned by `rotation` and rigs it
    // (RigCar). Invalid when it couldn't be loaded or rigged.
    Entity SpawnCar(Scene& scene, AssetManager2& assets, const std::string& model, const glm::vec3& position,
                    const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), float mass = 1500.0f);
}
