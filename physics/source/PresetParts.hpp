#pragma once
// Private to MarianaPhysics: building blocks of the presets (VehiclePresets, CharacterPresets).
#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>

#include <glm/gtc/quaternion.hpp>

namespace PhysicsInternal
{
    enum class Solid { None, Box, Cylinder, Sphere };

    inline const glm::quat kIdentity(1.0f, 0.0f, 0.0f, 0.0f);
    inline const glm::vec3 kX(1, 0, 0), kY(0, 1, 0), kZ(0, 0, 1);

    inline glm::quat Axis(float degrees, const glm::vec3& axis) { return glm::angleAxis(glm::radians(degrees), axis); }

    inline Entity Place(Entity e, Entity parent, const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
    {
        if (parent.IsValid())
            e.SetParent(parent);
        e.SetPosition(position.x, position.y, position.z);
        e.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
        e.SetScale(scale.x, scale.y, scale.z);
        return e;
    }

    // A primitive below `parent`, optionally with a collider fitted to it.
    inline Entity Part(Scene& scene, AssetManager2& assets, Entity parent, BuiltinMesh shape, const char* name, const glm::vec3& color,
                       const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, Solid solid = Solid::Box)
    {
        Entity e = BuiltinAssets::InstantiatePrimitive(scene, assets, shape, name, color);
        if (!e.IsValid())
            return e;
        Place(e, parent, position, rotation, scale);
        switch (solid)
        {
        case Solid::Box: e.Add<BoxCollider>(BoxCollider{}); break;
        case Solid::Cylinder: e.Add<CylinderCollider>(CylinderCollider{}); break;
        case Solid::Sphere: e.Add<SphereCollider>(SphereCollider{}); break;
        case Solid::None: break;
        }
        FitColliders(e);
        return e;
    }

    // A box tilted up toward +Z by `degrees`, its top surface meeting the ground at `lowEnd` (x, z).
    inline Entity Ramp(Scene& scene, AssetManager2& assets, Entity parent, const char* name, const glm::vec3& color, const glm::vec2& lowEnd,
                       float degrees, float length, float width, float thickness = 0.4f)
    {
        const float s = std::sin(glm::radians(degrees)), c = std::cos(glm::radians(degrees));
        // The top face's low edge is at the ground: center = edge + along the slope by half the
        // length, then down through half the thickness.
        const glm::vec3 along(0.0f, s, c), normal(0.0f, c, -s);
        const glm::vec3 center = glm::vec3(lowEnd.x, 0.0f, lowEnd.y) + along * (0.5f * length) - normal * (0.5f * thickness);
        return Part(scene, assets, parent, BuiltinMesh::Cube, name, color, center, Axis(-degrees, kX), glm::vec3(width, thickness, length));
    }
}
