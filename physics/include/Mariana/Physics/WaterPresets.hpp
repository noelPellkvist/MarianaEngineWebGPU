#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>

// Water made of the engine's primitives.
namespace WaterPresets
{
    // A WaterVolume of `size` with its top at `position` + half its height, a trigger reporting what
    // goes in, and its surface: a see-through, rippling plane (Blend, the builtin water normal map).
    Entity SpawnWater(Scene& scene, AssetManager2& assets, const glm::vec3& position, const glm::vec3& size, float waveHeight = 0.0f);
    // A 14 x 14 m pool, 2 m deep, standing on the ground at `center`: walls, a ramp up to its rim
    // (drive in), and things that float (wooden crates, a log, a raft) and one that sinks (a steel ball).
    Entity SpawnPool(Scene& scene, AssetManager2& assets, const glm::vec3& center);
}
