#pragma once
#include <glm/glm.hpp>

// The vertex every mesh the standard renderer draws uses (StandardPBR's vertex layout).
struct StandardVertex
{
    glm::vec3 position;
    glm::vec3 normal;      // zero for points and lines without normals (drawn unlit)
    glm::vec4 tangent;     // xyz + handedness in w (bitangent = cross(normal, tangent) * w)
    glm::vec2 texcoord0;
    glm::vec2 texcoord1;
    glm::vec4 color0;      // multiplies the base color; white when unused
    glm::uvec4 boneIndices; // into the mesh's skin joints
    glm::vec4 boneWeights;  // zero for meshes without a skin

    bool operator==(const StandardVertex& other) const = default;
};
