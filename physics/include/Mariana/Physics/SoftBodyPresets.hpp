#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Soft bodies made of the engine's builtin meshes (so scene files keep them), standing on the
// ground at `position`.
namespace SoftBodyPresets
{
    // A 4 m pole with a 2 x 1.2 m flag held along it, flying in a gusty wind that blows along the
    // flag (+X, turned by `rotation`).
    Entity SpawnFlag(Scene& scene, AssetManager2& assets, const glm::vec3& position, const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    // A 3 m square of cloth falling onto a ball on a pedestal and draping over it.
    Entity SpawnDrape(Scene& scene, AssetManager2& assets, const glm::vec3& position);
    // A 0.8 m cube of jelly (pumped up and springy) dropped from 1.5 m.
    Entity SpawnJelly(Scene& scene, AssetManager2& assets, const glm::vec3& position);
    // A 1 m wide ball of air, light and bouncy (a beach ball), dropped from 2 m.
    Entity SpawnBalloon(Scene& scene, AssetManager2& assets, const glm::vec3& position);
}
