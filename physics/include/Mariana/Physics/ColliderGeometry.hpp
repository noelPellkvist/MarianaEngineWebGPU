#pragma once
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <functional>
#include <memory>

enum class ColliderShape : uint8_t { Box, Sphere, Capsule, Cylinder, Mesh, ConvexHull };

// A collider as the simulation sees it: its shape placed by its entity's transform, the entity's
// scale applied (boxes per axis; spheres, capsules and cylinders keep round, so they take the
// largest scale across them; meshes keep it as `scale`). What bodies are made of, and what editors
// draw.
struct ColliderGeometry
{
    ColliderShape shape = ColliderShape::Box;
    glm::vec3 position{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 halfExtents{ 0.5f }; // box
    float radius = 0.5f;           // sphere, capsule, cylinder
    float halfHeight = 0.5f;       // capsule: of its straight part; cylinder: half its height
    // Mesh and convex hull: the triangles, their object-space bounds and the scale to apply.
    std::shared_ptr<const MeshCollisionData> mesh;
    Bounds meshBounds;
    glm::vec3 scale{ 1.0f };
    const Collider* collider = nullptr;
};

// Every collider on `entity`, placed by `transform` (its world matrix, or relative to a body). A
// MeshCollider whose mesh has no triangles (or isn't loaded) gives nothing.
void ForEachCollider(Entity entity, const glm::mat4& transform, const std::function<void(const ColliderGeometry&)>& fn);
bool HasCollider(Entity entity);
// The mesh a MeshCollider uses: its own, or the entity's MeshComponent mesh.
const IMesh* ColliderMesh(Entity entity, const MeshCollider& collider);

// The bounds of the entity's meshes in its own space: its mesh's, or for a model's root those of
// every mesh below it (placed by their WorldXform). Empty when there are none.
Bounds MeshBounds(Entity entity);
// Sizes and centres the entity's box, sphere, capsule and cylinder colliders to MeshBounds (a flat
// mesh, a ground plane, gets a 0.1 thick slab under its surface); false when there are no meshes.
bool FitColliders(Entity entity);
