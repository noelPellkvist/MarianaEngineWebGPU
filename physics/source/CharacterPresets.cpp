#include <Mariana/Physics/CharacterPresets.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Scene/Prefab.hpp>

#include <Mariana/Physics/Character.hpp>
#include <Mariana/Physics/Controllers.hpp>

#include "PresetParts.hpp"

#include <string>

using namespace PhysicsInternal;

namespace CharacterPresets
{
    Entity Spawn(Scene& scene, AssetManager2& assets, const glm::vec3& position, const glm::quat& rotation)
    {
        Entity character = scene.Instantiate("Character");
        Place(character, Entity{}, position, rotation, glm::vec3(1.0f));
        character.Add<CharacterController>(CharacterController{});
        character.Add<PlayerControl>(PlayerControl{});
        // The capsule primitive is 1 m wide and 2 m tall: scaled to 0.6 by 1.8, standing on the feet.
        Part(scene, assets, character, BuiltinMesh::Capsule, "Body", glm::vec3(0.25f, 0.55f, 0.9f), glm::vec3(0.0f, 0.9f, 0.0f), kIdentity, glm::vec3(0.6f, 0.9f, 0.6f), Solid::None);
        Part(scene, assets, character, BuiltinMesh::Cube, "Visor", glm::vec3(0.08f), glm::vec3(0.0f, 1.55f, 0.24f), kIdentity, glm::vec3(0.4f, 0.12f, 0.14f), Solid::None);
        return character;
    }

    Entity SpawnCourse(Scene& scene, AssetManager2& assets, const glm::vec3& center)
    {
        Entity root = scene.Instantiate("Character Course");
        root.SetPosition(center.x, center.y, center.z);
        const glm::vec3 grass(0.32f, 0.4f, 0.27f), stone(0.6f, 0.58f, 0.55f), steep(0.7f, 0.35f, 0.3f), orange(0.85f, 0.45f, 0.15f);

        Part(scene, assets, root, BuiltinMesh::Plane, "Ground", grass, glm::vec3(0.0f), kIdentity, glm::vec3(6.0f));

        // Stairs: 8 steps of 0.25 m up and 0.45 m deep, then a landing.
        for (int i = 0; i < 8; ++i)
        {
            const float h = 0.25f * static_cast<float>(i + 1);
            const std::string name = "Stair " + std::to_string(i + 1);
            Part(scene, assets, root, BuiltinMesh::Cube, name.c_str(), stone, glm::vec3(-8.0f, 0.5f * h, 3.0f + 0.45f * static_cast<float>(i) + 0.225f), kIdentity, glm::vec3(3.0f, h, 0.45f));
        }
        Part(scene, assets, root, BuiltinMesh::Cube, "Landing", stone, glm::vec3(-8.0f, 1.0f, 3.0f + 3.6f + 1.5f), kIdentity, glm::vec3(3.0f, 2.0f, 3.0f));

        // One 0.3 m step.
        Part(scene, assets, root, BuiltinMesh::Cube, "Step", stone, glm::vec3(-4.0f, 0.15f, 5.0f), kIdentity, glm::vec3(3.0f, 0.3f, 4.0f));

        // A slope it walks up and one too steep.
        Ramp(scene, assets, root, "Slope 30", stone, glm::vec2(0.0f, 3.0f), 30.0f, 8.0f, 3.0f);
        Ramp(scene, assets, root, "Slope 60", steep, glm::vec2(4.0f, 3.0f), 60.0f, 6.0f, 3.0f);

        // Crates to push.
        for (int i = 0; i < 3; ++i)
        {
            Entity crate = Part(scene, assets, root, BuiltinMesh::Cube, "Crate", orange, glm::vec3(8.0f, 0.4f, 4.0f + 1.2f * static_cast<float>(i)), kIdentity, glm::vec3(0.8f));
            RigidBody body;
            body.mass = 30.0f;
            crate.Add<RigidBody>(body);
        }

        // A zone to walk through: a trigger with no mesh, a pad on the floor to show where.
        Entity zone = Place(scene.Instantiate("Zone"), root, glm::vec3(12.0f, 1.0f, 6.0f), kIdentity, glm::vec3(1.0f));
        BoxCollider trigger;
        trigger.size = glm::vec3(3.0f, 2.0f, 3.0f);
        trigger.isTrigger = true;
        zone.Add<BoxCollider>(trigger);
        Part(scene, assets, zone, BuiltinMesh::Cube, "Pad", glm::vec3(0.3f, 0.85f, 0.9f), glm::vec3(0.0f, -0.99f, 0.0f), kIdentity, glm::vec3(3.0f, 0.02f, 3.0f), Solid::None);
        return root;
    }

    Entity SpawnRagdoll(Scene& scene, AssetManager2& assets, const glm::vec3& position, const glm::quat& rotation, RagdollMode mode, const std::string& model)
    {
        const std::filesystem::path file = FileSystem::Resolve(model);
        std::shared_ptr<Prefab> prefab = file.empty() ? nullptr : assets.Load<Prefab>(file).Get();
        if (!prefab)
        {
            Logger::Warning("SpawnRagdoll: couldn't load " + model);
            return {};
        }
        Entity root = scene.Instantiate(*prefab, file.stem().string().c_str());
        root.SetPosition(position.x, position.y, position.z);
        root.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
        Ragdoll ragdoll;
        ragdoll.mode = mode;
        root.Add<Ragdoll>(ragdoll);
        scene.Update(0.0f);
        return root;
    }
}
