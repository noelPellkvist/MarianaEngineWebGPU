#include <Mariana/Physics/SoftBodyPresets.hpp>
#include <Mariana/Physics/SoftBody.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>

#include "PresetParts.hpp"

using namespace PhysicsInternal;

namespace
{
    // A soft body of a builtin mesh, its material double sided (cloth shows both sides).
    Entity SoftPart(Scene& scene, AssetManager2& assets, Entity parent, BuiltinMesh shape, const char* name, const glm::vec3& color,
                    const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, const SoftBody& soft, float roughness = 0.8f)
    {
        Entity e = Part(scene, assets, parent, shape, name, color, position, rotation, scale, Solid::None);
        if (!e.IsValid())
            return e;
        if (MaterialAsset* material = e.Get<MeshComponent>()->materials[0].Get().get())
        {
            material->doubleSided = true;
            material->properties.roughnessFactor = roughness;
            material->MarkDirty();
        }
        e.Add<SoftBody>(soft);
        return e;
    }
}

namespace SoftBodyPresets
{
    Entity SpawnFlag(Scene& scene, AssetManager2& assets, const glm::vec3& position, const glm::quat& rotation)
    {
        Entity root = Place(scene.Instantiate("Flag"), {}, position, rotation, glm::vec3(1.0f));
        Part(scene, assets, root, BuiltinMesh::Cylinder, "Pole", glm::vec3(0.75f), glm::vec3(0.0f, 2.0f, 0.0f), kIdentity, glm::vec3(0.1f, 4.0f, 0.1f), Solid::Cylinder);
        SoftBody cloth;
        cloth.mass = 0.3f;
        cloth.stiffness = 0.9f;
        cloth.bendStiffness = 0.1f;
        cloth.thickness = 0.01f;
        cloth.pin = SoftBodyPin::Left;
        cloth.wind = rotation * glm::vec3(8.0f, 0.0f, 0.0f);
        cloth.gusts = 0.6f;
        // The grid stood up (its +Y face toward +Z), its left edge along the pole.
        SoftPart(scene, assets, root, BuiltinMesh::Grid, "Flag Cloth", glm::vec3(0.8f, 0.1f, 0.08f),
                 glm::vec3(1.06f, 3.35f, 0.0f), Axis(90.0f, kX), glm::vec3(2.0f, 1.0f, 1.2f), cloth);
        return root;
    }

    Entity SpawnDrape(Scene& scene, AssetManager2& assets, const glm::vec3& position)
    {
        Entity root = Place(scene.Instantiate("Drape"), {}, position, kIdentity, glm::vec3(1.0f));
        Part(scene, assets, root, BuiltinMesh::Cylinder, "Pedestal", glm::vec3(0.55f), glm::vec3(0.0f, 0.5f, 0.0f), kIdentity, glm::vec3(0.6f, 1.0f, 0.6f), Solid::Cylinder);
        Part(scene, assets, root, BuiltinMesh::Sphere, "Ball", glm::vec3(0.9f, 0.85f, 0.75f), glm::vec3(0.0f, 1.6f, 0.0f), kIdentity, glm::vec3(1.2f), Solid::Sphere);
        SoftBody cloth;
        cloth.mass = 1.0f;
        cloth.stiffness = 0.9f;
        cloth.bendStiffness = 0.02f;
        cloth.friction = 0.8f;
        cloth.thickness = 0.03f;
        SoftPart(scene, assets, root, BuiltinMesh::Grid, "Cloth", glm::vec3(0.15f, 0.3f, 0.8f), glm::vec3(0.0f, 2.9f, 0.0f), kIdentity, glm::vec3(3.0f, 1.0f, 3.0f), cloth);
        return root;
    }

    Entity SpawnJelly(Scene& scene, AssetManager2& assets, const glm::vec3& position)
    {
        SoftBody jelly;
        jelly.mass = 5.0f;
        jelly.stiffness = 0.9f;
        jelly.bendStiffness = 0.9f;
        jelly.pressure = 8.0f;
        jelly.restitution = 0.2f;
        jelly.damping = 0.3f;
        jelly.friction = 0.6f;
        return SoftPart(scene, assets, {}, BuiltinMesh::SubdividedCube, "Jelly", glm::vec3(0.3f, 0.85f, 0.25f), position + glm::vec3(0.0f, 1.5f, 0.0f), kIdentity, glm::vec3(0.8f), jelly, 0.25f);
    }

    Entity SpawnBalloon(Scene& scene, AssetManager2& assets, const glm::vec3& position)
    {
        SoftBody ball;
        ball.mass = 0.3f;
        ball.stiffness = 0.9f;
        ball.bendStiffness = 0.3f;
        ball.pressure = 20.0f;
        ball.restitution = 0.6f;
        ball.damping = 0.3f;
        return SoftPart(scene, assets, {}, BuiltinMesh::Sphere, "Balloon", glm::vec3(0.95f, 0.75f, 0.1f), position + glm::vec3(0.0f, 2.0f, 0.0f), kIdentity, glm::vec3(1.0f), ball, 0.4f);
    }
}
