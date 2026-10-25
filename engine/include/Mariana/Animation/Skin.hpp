#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Assets/AssetManager.hpp>

// Joints of a glTF skin: the node driving each joint, and the joint's inverse bind matrix.
struct Skin
{
    std::vector<uint32_t> jointNodes;
    std::vector<glm::mat4> inverseBind;
};

// On an entity whose mesh is skinned. Its vertices' boneIndices index into the skin's joints. What
// the renderer works out from it each frame is the entity's SkinPose (Rendering/Components.hpp).
struct SkinComponent
{
    AssetHandle<Skin> skin;

    SkinComponent() = default;
    explicit SkinComponent(AssetHandle<Skin> skin) : skin(std::move(skin)) {}
};
