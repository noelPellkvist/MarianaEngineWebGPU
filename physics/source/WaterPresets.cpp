#include <Mariana/Physics/WaterPresets.hpp>

#include <Mariana/Physics/Water.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>

#include "PresetParts.hpp"

#include <cmath>

using namespace PhysicsInternal;

namespace
{
    // A dynamic part whose mass comes from its collider's density (kg/m^3), so water judges it right.
    Entity Floating(Scene& scene, AssetManager2& assets, Entity parent, BuiltinMesh shape, const char* name, const glm::vec3& color,
                    const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, Solid solid, float density)
    {
        Entity e = Part(scene, assets, parent, shape, name, color, position, rotation, scale, solid);
        if (BoxCollider* c = e.Get<BoxCollider>())
            c->density = density;
        if (CylinderCollider* c = e.Get<CylinderCollider>())
            c->density = density;
        if (SphereCollider* c = e.Get<SphereCollider>())
            c->density = density;
        RigidBody body;
        body.mass = 0.0f;
        e.Add<RigidBody>(body);
        return e;
    }
}

namespace WaterPresets
{
    Entity SpawnWater(Scene& scene, AssetManager2& assets, const glm::vec3& position, const glm::vec3& size, float waveHeight)
    {
        Entity water = scene.Instantiate("Water");
        water.SetPosition(position.x, position.y, position.z);
        WaterVolume volume;
        volume.size = size;
        volume.waveHeight = waveHeight;
        water.Add<WaterVolume>(volume);
        BoxCollider trigger;
        trigger.size = size;
        trigger.isTrigger = true;
        water.Add<BoxCollider>(trigger);

        // The surface: the 10 m plane primitive over the top, seen from above and below.
        Entity surface = BuiltinAssets::InstantiatePrimitive(scene, assets, BuiltinMesh::Plane, "Surface", glm::vec3(0.03f, 0.17f, 0.2f));
        if (surface.IsValid())
        {
            Place(surface, water, glm::vec3(0.0f, 0.5f * size.y, 0.0f), kIdentity, glm::vec3(size.x / 10.0f, 1.0f, size.z / 10.0f));
            surface.RemoveTag<ShadowCasterTag>();
            if (MaterialAsset* m = surface.Get<MeshComponent>()->materials[0].Get().get())
            {
                m->SetAlphaMode(AlphaMode::Blend);
                m->properties.baseColor.a = 0.6f;
                m->properties.roughnessFactor = 0.06f;
                m->properties.normalMapStrength = 0.5f;
                m->doubleSided = true;
                m->SetTexture(MaterialTexture::Normal, BuiltinAssets::WaterNormalTexture(assets));
                MaterialAsset::TextureTransform tiles;
                tiles.scale = glm::vec2(size.x, size.z) / 3.0f; // a ripple pattern every 3 m
                m->SetTextureTransform(MaterialTexture::Normal, tiles);
                m->MarkDirty();
            }
        }
        return water;
    }

    Entity SpawnPool(Scene& scene, AssetManager2& assets, const glm::vec3& center)
    {
        Entity pool = scene.Instantiate("Pool");
        pool.SetPosition(center.x, center.y, center.z);
        const glm::vec3 stone(0.62f, 0.62f, 0.6f), floor(0.25f, 0.45f, 0.5f);
        const float inner = 7.0f, wall = 0.6f, height = 2.2f, depth = 2.0f;
        const float out = inner + 0.5f * wall;
        Part(scene, assets, pool, BuiltinMesh::Cube, "Floor", floor, glm::vec3(0.0f, -0.2f, 0.0f), kIdentity, glm::vec3(2.0f * (inner + wall), 0.4f, 2.0f * (inner + wall)));
        Part(scene, assets, pool, BuiltinMesh::Cube, "Wall N", stone, glm::vec3(0.0f, 0.5f * height, out), kIdentity, glm::vec3(2.0f * (inner + wall), height, wall));
        Part(scene, assets, pool, BuiltinMesh::Cube, "Wall S", stone, glm::vec3(0.0f, 0.5f * height, -out), kIdentity, glm::vec3(2.0f * (inner + wall), height, wall));
        Part(scene, assets, pool, BuiltinMesh::Cube, "Wall E", stone, glm::vec3(out, 0.5f * height, 0.0f), kIdentity, glm::vec3(wall, height, 2.0f * inner));
        Part(scene, assets, pool, BuiltinMesh::Cube, "Wall W", stone, glm::vec3(-out, 0.5f * height, 0.0f), kIdentity, glm::vec3(wall, height, 2.0f * inner));
        // Up to the south rim, to drive in.
        const float run = 12.0f;
        const float degrees = glm::degrees(std::atan2(height, run));
        Ramp(scene, assets, pool, "Ramp", stone, glm::vec2(0.0f, -(inner + wall) - run), degrees, std::sqrt(run * run + height * height), 6.0f);

        Entity water = SpawnWater(scene, assets, glm::vec3(0.0f, 0.5f * depth, 0.0f), glm::vec3(2.0f * inner, depth, 2.0f * inner), 0.06f);
        water.SetParent(pool);

        // Wood floats about half under, the steel ball goes to the bottom.
        const glm::vec3 wood(0.55f, 0.36f, 0.18f), bark(0.36f, 0.24f, 0.13f);
        for (int i = 0; i < 4; ++i)
        {
            const float x = i % 2 ? 2.5f : -2.5f, z = i < 2 ? 2.5f : -2.5f;
            Floating(scene, assets, pool, BuiltinMesh::Cube, "Crate", wood, glm::vec3(x, depth + 0.5f, z), Axis(20.0f * i, kY), glm::vec3(0.8f), Solid::Box, 500.0f);
        }
        Floating(scene, assets, pool, BuiltinMesh::Cylinder, "Log", bark, glm::vec3(0.0f, depth + 0.5f, 4.5f), Axis(90.0f, kZ), glm::vec3(0.6f, 3.0f, 0.6f), Solid::Cylinder, 600.0f);
        Floating(scene, assets, pool, BuiltinMesh::Sphere, "Steel Ball", glm::vec3(0.3f), glm::vec3(-4.5f, depth + 0.6f, 0.0f), kIdentity, glm::vec3(0.8f), Solid::Sphere, 7800.0f);

        // A raft: planks side by side, one body.
        Entity raft = Place(scene.Instantiate("Raft"), pool, glm::vec3(3.8f, depth + 0.3f, -3.5f), Axis(15.0f, kY), glm::vec3(1.0f));
        RigidBody body;
        body.mass = 0.0f;
        raft.Add<RigidBody>(body);
        for (int i = 0; i < 5; ++i)
        {
            Entity plank = Part(scene, assets, raft, BuiltinMesh::Cube, "Plank", i % 2 ? wood : bark, glm::vec3(-1.0f + 0.5f * i, 0.0f, 0.0f), kIdentity, glm::vec3(0.46f, 0.14f, 2.4f));
            if (BoxCollider* c = plank.Get<BoxCollider>())
                c->density = 500.0f;
        }
        return pool;
    }
}
