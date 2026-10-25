#pragma once
#include <cstdint>
#include <vector>

#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Scene/ECS.hpp>

class IMesh;
class MaterialAsset;

// Draws `mesh` with the scene renderer.
struct MeshComponent {
    AssetHandle<IMesh> mesh;
    // Material for each submesh, indexed by Submesh::materialIndex. Missing or empty entries
    // render with the default material.
    std::vector<AssetHandle<MaterialAsset>> materials;
};

// The entity's mesh casts shadows.
struct ShadowCasterTag {};

// A mesh drawn instead of the MeshComponent's while the entity has it: a runtime component another
// module gives an entity whose mesh it changes as it runs (physics, a soft body's deforming copy).
// It has the same submeshes, so the MeshComponent's materials still apply.
struct MeshOverride {
    AssetHandle<IMesh> mesh;
};

// What the scene renderer works out each frame for an entity with a SkinComponent: a runtime
// component (ECS::RegisterRuntimeComponent) it gives every such entity, read by the views.
struct SkinPose {
    std::vector<Entity> joints;  // the joints' entities, as Skin::jointNodes (found once)
    uint32_t boneOffset = ~0u;   // where this frame's joint matrices start in the bone buffer
    uint32_t poseVersion = 0;    // changes with the pose, so shadows know when to redraw
    Bounds bounds;               // the posed mesh in world space (empty = unknown, never culled)
};

// A camera in the scene. It looks along its entity's +z axis with +y up (SceneCamera); the
// projection is perspective.
struct CameraComponent {
    float fov = 60.0f;      // vertical, degrees
    float nearClip = 0.1f;
    float farClip = 500.0f;
};
