#include <Mariana/Physics/ColliderGeometry.hpp>

#include <algorithm>
#include <cmath>

namespace
{
    // The transform split into its parts: columns' lengths as the scale (x negative when it
    // mirrors), and the rotation of the normalized columns.
    void Decompose(const glm::mat4& m, glm::vec3& position, glm::quat& rotation, glm::vec3& scale)
    {
        position = glm::vec3(m[3]);
        scale = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
        if (glm::determinant(glm::mat3(m)) < 0.0f)
            scale.x = -scale.x;
        glm::mat3 axes(1.0f);
        for (int i = 0; i < 3; ++i)
            if (std::abs(scale[i]) > 1e-8f)
                axes[i] = glm::vec3(m[i]) / scale[i];
        rotation = glm::normalize(glm::quat_cast(axes));
    }

    // How long the collider's local axis `axis` gets under the entity's scale.
    float ScaledLength(const glm::vec3& scale, const glm::quat& rotation, const glm::vec3& axis)
    {
        return glm::length(scale * (rotation * axis));
    }

    ColliderGeometry Place(const glm::mat4& transform, const Collider& collider, ColliderShape shape, glm::vec3& axisScale)
    {
        glm::vec3 position, scale;
        glm::quat rotation;
        Decompose(transform, position, rotation, scale);

        ColliderGeometry geometry;
        geometry.shape = shape;
        geometry.collider = &collider;
        geometry.position = position + rotation * (scale * collider.center);
        geometry.rotation = glm::normalize(rotation * collider.rotation);
        axisScale = glm::vec3(ScaledLength(scale, collider.rotation, glm::vec3(1, 0, 0)),
                              ScaledLength(scale, collider.rotation, glm::vec3(0, 1, 0)),
                              ScaledLength(scale, collider.rotation, glm::vec3(0, 0, 1)));
        return geometry;
    }

    constexpr float kMinSize = 1e-3f;
}

void ForEachCollider(Entity entity, const glm::mat4& transform, const std::function<void(const ColliderGeometry&)>& fn)
{
    glm::vec3 s;
    if (const BoxCollider* box = entity.TryGet<BoxCollider>())
    {
        ColliderGeometry g = Place(transform, *box, ColliderShape::Box, s);
        g.halfExtents = glm::max(glm::abs(box->size) * s * 0.5f, glm::vec3(kMinSize));
        fn(g);
    }
    if (const SphereCollider* sphere = entity.TryGet<SphereCollider>())
    {
        ColliderGeometry g = Place(transform, *sphere, ColliderShape::Sphere, s);
        g.radius = std::max(std::abs(sphere->radius) * std::max({ s.x, s.y, s.z }), kMinSize);
        fn(g);
    }
    if (const CapsuleCollider* capsule = entity.TryGet<CapsuleCollider>())
    {
        ColliderGeometry g = Place(transform, *capsule, ColliderShape::Capsule, s);
        g.radius = std::max(std::abs(capsule->radius) * std::max(s.x, s.z), kMinSize);
        g.halfHeight = std::max(std::abs(capsule->height) * s.y * 0.5f - g.radius, 0.0f);
        fn(g);
    }
    if (const CylinderCollider* cylinder = entity.TryGet<CylinderCollider>())
    {
        ColliderGeometry g = Place(transform, *cylinder, ColliderShape::Cylinder, s);
        g.radius = std::max(std::abs(cylinder->radius) * std::max(s.x, s.z), kMinSize);
        g.halfHeight = std::max(std::abs(cylinder->height) * s.y * 0.5f, kMinSize);
        fn(g);
    }
}

bool HasCollider(Entity entity)
{
    return entity.TryGet<BoxCollider>() || entity.TryGet<SphereCollider>() || entity.TryGet<CapsuleCollider>() || entity.TryGet<CylinderCollider>();
}
