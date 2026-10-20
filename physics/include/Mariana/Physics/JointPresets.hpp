#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>

// Joints at work, made of the engine's primitives.
namespace JointPresets
{
    // A chain of `links` capsules hanging from `top` (the first held to the world, each next one to
    // the one above it: nested bodies with joints `toParent`).
    Entity SpawnChain(Scene& scene, AssetManager2& assets, const glm::vec3& top, int links = 10);
    // A door on a hinge with limits and a little friction, in a frame, its hinge side at `position`.
    Entity SpawnDoor(Scene& scene, AssetManager2& assets, const glm::vec3& position);
    // A turntable driven round by a hinge motor, with a few crates on it.
    Entity SpawnSpinner(Scene& scene, AssetManager2& assets, const glm::vec3& position);
    // A 500 kg ball on a rope (a distance joint) from a point 7 m up, pulled aside to swing into a
    // stack of crates.
    Entity SpawnWreckingBall(Scene& scene, AssetManager2& assets, const glm::vec3& position);
}
