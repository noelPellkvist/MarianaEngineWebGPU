#pragma once
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <functional>

enum class ColliderShape : uint8_t { Box, Sphere, Capsule, Cylinder };

// A collider as the simulation sees it: its shape placed by its entity's transform, the entity's
// scale applied (boxes per axis; spheres, capsules and cylinders keep round, so they take the
// largest scale across them). What bodies are made of, and what editors draw.
struct ColliderGeometry
{
    ColliderShape shape = ColliderShape::Box;
    glm::vec3 position{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 halfExtents{ 0.5f }; // box
    float radius = 0.5f;           // sphere, capsule, cylinder
    float halfHeight = 0.5f;       // capsule: of its straight part; cylinder: half its height
    const Collider* collider = nullptr;
};

// Every collider on `entity`, placed by `transform` (its world matrix, or relative to a body).
void ForEachCollider(Entity entity, const glm::mat4& transform, const std::function<void(const ColliderGeometry&)>& fn);
bool HasCollider(Entity entity);
