#pragma once
// Bounds and frustum helpers shared by SceneRenderer (skinning) and SceneView (culling).
#include <Mariana/Rendering/SceneView.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Animation/Skin.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace SceneCulling
{
    // Planes of a view-projection matrix with [0, 1] clip depth (Gribb/Hartmann), pointing inwards.
    inline Frustum ExtractFrustum(const glm::mat4& viewProj)
    {
        const glm::vec4 row0(viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]);
        const glm::vec4 row1(viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]);
        const glm::vec4 row2(viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]);
        const glm::vec4 row3(viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]);

        Frustum frustum;
        frustum.planes[0] = row3 + row0; // left
        frustum.planes[1] = row3 - row0; // right
        frustum.planes[2] = row3 + row1; // bottom
        frustum.planes[3] = row3 - row1; // top
        frustum.planes[4] = row2;        // near
        frustum.planes[5] = row3 - row2; // far
        return frustum;
    }

    // Axis-aligned box around `box` moved by `m`.
    inline Bounds TransformBounds(const glm::mat4& m, const Bounds& box)
    {
        const glm::vec3 localCenter = (box.min + box.max) * 0.5f;
        const glm::vec3 localExtent = (box.max - box.min) * 0.5f;

        const glm::vec3 center = glm::vec3(m * glm::vec4(localCenter, 1.0f));
        const glm::vec3 extent = glm::abs(glm::vec3(m[0])) * localExtent.x
                               + glm::abs(glm::vec3(m[1])) * localExtent.y
                               + glm::abs(glm::vec3(m[2])) * localExtent.z;
        return { center - extent, center + extent };
    }

    // Whether a world-space box is at least partly inside the frustum.
    inline bool IsVisible(const Frustum& frustum, const Bounds& box)
    {
        const glm::vec3 center = (box.min + box.max) * 0.5f;
        const glm::vec3 extent = (box.max - box.min) * 0.5f;
        for (const glm::vec4& plane : frustum.planes)
        {
            const glm::vec3 normal(plane);
            if (glm::dot(normal, center) + plane.w < -glm::dot(glm::abs(normal), extent))
                return false;
        }
        return true;
    }

    // Whether the mesh, placed by `model`, is at least partly inside the frustum. Skinned meshes move
    // beyond their bind-pose bounds, so they use the posed bounds the skinning pass computed instead.
    inline bool IsVisible(const Frustum& frustum, const IMesh& mesh, const float* model, const SkinComponent* skin)
    {
        if (skin)
            return skin->bounds.IsEmpty() || IsVisible(frustum, skin->bounds);
        if (!mesh.hasBounds)
            return true;
        return IsVisible(frustum, TransformBounds(glm::make_mat4(model), Bounds{ mesh.boundsMin, mesh.boundsMax }));
    }
}
