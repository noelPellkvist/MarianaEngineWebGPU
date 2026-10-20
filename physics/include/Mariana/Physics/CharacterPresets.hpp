#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// A ready-to-walk character and a course to try it on, made of the engine's primitives.
namespace CharacterPresets
{
    // A 1.8 m capsule with a visor on its front (+Z), a CharacterController and a PlayerControl, its
    // feet at `position`.
    Entity Spawn(Scene& scene, AssetManager2& assets, const glm::vec3& position, const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // A 60 m ground with lanes starting at z = 3 in front of `center`, each 4 m apart along x: stairs
    // (x -8, 0.25 m steps), a 0.3 m step (x -4), a 30 degree slope it walks up (x 0), a 60 degree one
    // it can't (x 4), crates to push (x 8) and a trigger zone to walk through (x 12).
    Entity SpawnCourse(Scene& scene, AssetManager2& assets, const glm::vec3& center);
}
