#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Rendering/Components.hpp>

#include <glm/gtc/type_ptr.hpp>

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
    if (const MeshCollider* meshCollider = entity.TryGet<MeshCollider>())
    {
        const IMesh* mesh = ColliderMesh(entity, *meshCollider);
        if (mesh && mesh->collision)
        {
            glm::vec3 position, scale;
            glm::quat rotation;
            Decompose(transform, position, rotation, scale);
            ColliderGeometry g;
            g.shape = meshCollider->convex ? ColliderShape::ConvexHull : ColliderShape::Mesh;
            g.collider = meshCollider;
            g.position = position + rotation * (scale * meshCollider->center);
            g.rotation = glm::normalize(rotation * meshCollider->rotation);
            g.mesh = mesh->collision;
            g.meshBounds.min = mesh->boundsMin;
            g.meshBounds.max = mesh->boundsMax;
            for (int i = 0; i < 3; ++i)
                g.scale[i] = std::abs(scale[i]) < kMinSize ? std::copysign(kMinSize, scale[i]) : scale[i];
            fn(g);
        }
    }
}

namespace
{
    const IMesh* MeshOf(Entity entity)
    {
        const MeshComponent* mesh = entity.TryGet<MeshComponent>();
        return mesh && mesh->mesh.Get() && mesh->mesh.Get()->hasBounds ? mesh->mesh.Get().get() : nullptr;
    }
}

Bounds MeshBounds(Entity entity)
{
    Bounds bounds;
    if (const IMesh* mesh = MeshOf(entity))
    {
        bounds.Add(mesh->boundsMin);
        bounds.Add(mesh->boundsMax);
        return bounds;
    }
    const WorldXform* root = entity.TryGet<WorldXform>();
    if (!root)
        return bounds;
    const glm::mat4 toEntity = glm::inverse(glm::make_mat4(root->model));
    std::function<void(Entity)> visit = [&](Entity e) {
        const IMesh* mesh = MeshOf(e);
        const WorldXform* world = e.TryGet<WorldXform>();
        if (mesh && world)
        {
            const glm::mat4 m = toEntity * glm::make_mat4(world->model);
            for (int i = 0; i < 8; ++i)
            {
                const glm::vec3 corner((i & 1) ? mesh->boundsMax.x : mesh->boundsMin.x, (i & 2) ? mesh->boundsMax.y : mesh->boundsMin.y, (i & 4) ? mesh->boundsMax.z : mesh->boundsMin.z);
                bounds.Add(glm::vec3(m * glm::vec4(corner, 1.0f)));
            }
        }
        e.ForEachChild(visit);
    };
    entity.ForEachChild(visit);
    return bounds;
}

bool FitColliders(Entity entity)
{
    const Bounds bounds = MeshBounds(entity);
    if (bounds.IsEmpty())
        return false;
    glm::vec3 size = bounds.max - bounds.min, center = (bounds.min + bounds.max) * 0.5f;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (size[axis] < 1e-3f)
        {
            size[axis] = 0.1f;
            center[axis] -= 0.05f;
        }
    }
    if (BoxCollider* box = entity.Get<BoxCollider>())
    {
        box->size = size;
        box->center = center;
    }
    if (SphereCollider* sphere = entity.Get<SphereCollider>())
    {
        sphere->radius = 0.5f * std::max({ size.x, size.y, size.z });
        sphere->center = center;
    }
    if (CapsuleCollider* capsule = entity.Get<CapsuleCollider>())
    {
        capsule->radius = 0.5f * std::max(size.x, size.z);
        capsule->height = std::max(size.y, 2.0f * capsule->radius);
        capsule->center = center;
    }
    if (CylinderCollider* cylinder = entity.Get<CylinderCollider>())
    {
        cylinder->radius = 0.5f * std::max(size.x, size.z);
        cylinder->height = size.y;
        cylinder->center = center;
    }
    return true;
}

const IMesh* ColliderMesh(Entity entity, const MeshCollider& collider)
{
    if (collider.mesh.IsValid())
        return collider.mesh.Get().get();
    const MeshComponent* component = entity.TryGet<MeshComponent>();
    return component && component->mesh.IsValid() ? component->mesh.Get().get() : nullptr;
}

bool HasCollider(Entity entity)
{
    return entity.TryGet<BoxCollider>() || entity.TryGet<SphereCollider>() || entity.TryGet<CapsuleCollider>() || entity.TryGet<CylinderCollider>() ||
           entity.TryGet<MeshCollider>();
}
