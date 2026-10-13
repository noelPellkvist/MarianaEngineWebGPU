#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include <ECS.hpp>
#include <Mesh.hpp>
#include <AssetsSystem/AssetManager.hpp>

// Joints of a glTF skin: the node driving each joint, and the joint's inverse bind matrix.
struct Skin
{
    std::vector<uint32_t> jointNodes;
    std::vector<glm::mat4> inverseBind;
};

// On an entity whose mesh is skinned. Its vertices' boneIndices index into the skin's joints.
struct SkinComponent
{
    AssetHandle<Skin> skin;

    // Resolved on first use (same order as Skin::jointNodes), like AnimationPlayer's targets.
    std::vector<Entity> joints;
    // Where this frame's joint matrices start in the bone buffer, set by the skinning pass.
    uint32_t boneOffset = ~0u;
    // Sum of the joints' WorldXform versions, so shadows know when the pose changed.
    uint32_t poseVersion = 0;
    // World-space bounds of the posed mesh, from the mesh's jointBounds (empty = unknown, not culled).
    Bounds bounds;

    SkinComponent() = default;
    explicit SkinComponent(AssetHandle<Skin> skin) : skin(std::move(skin)) {}
    // Copies only the skin; resolved joints belong to the source's entities (prefab clones).
    SkinComponent(const SkinComponent& other) : skin(other.skin) {}
    SkinComponent& operator=(const SkinComponent& other)
    {
        if (this == &other)
            return *this;
        skin = other.skin;
        joints.clear();
        boneOffset = ~0u;
        poseVersion = 0;
        bounds = {};
        return *this;
    }
    SkinComponent(SkinComponent&&) = default;
    SkinComponent& operator=(SkinComponent&&) = default;
};
