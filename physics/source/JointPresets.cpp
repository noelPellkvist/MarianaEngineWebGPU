#include <Mariana/Physics/JointPresets.hpp>

#include <Mariana/Physics/Joint.hpp>

#include "PresetParts.hpp"

#include <string>

using namespace PhysicsInternal;

namespace
{
    // A joint on its own entity below `parent` (the body), at `position` in the parent's space with
    // its X axis turned by `rotation`.
    Entity JointAt(Scene& scene, Entity parent, const glm::vec3& position, const glm::quat& rotation, const Joint& joint)
    {
        Entity e = Place(scene.Instantiate("Joint"), parent, position, rotation, glm::vec3(1.0f));
        e.Add<Joint>(joint);
        return e;
    }

    // Joint X axis straight up.
    const glm::quat kXUp = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
}

namespace JointPresets
{
    Entity SpawnChain(Scene& scene, AssetManager2& assets, const glm::vec3& top, int links)
    {
        Entity root = scene.Instantiate("Chain");
        root.SetPosition(top.x, top.y, top.z);
        // A beam to hang it from (looks only).
        Part(scene, assets, root, BuiltinMesh::Cube, "Beam", glm::vec3(0.35f), glm::vec3(0.0f, 0.1f, 0.0f), kIdentity, glm::vec3(1.0f, 0.2f, 0.2f), Solid::None);

        const float length = 0.5f, radius = 0.07f;
        Entity above = root;
        for (int i = 0; i < links; ++i)
        {
            // Each link hangs below the last, its centre half a link under the joint at its top. The
            // links nest, so they stay unscaled (their meshes are scaled below them).
            const std::string name = "Link " + std::to_string(i + 1);
            Entity link = Place(scene.Instantiate(name.c_str()), above, glm::vec3(0.0f, i == 0 ? -0.5f * length : -length, 0.0f), kIdentity, glm::vec3(1.0f));
            Part(scene, assets, link, BuiltinMesh::Capsule, "Mesh", glm::vec3(0.75f, 0.7f, 0.6f), glm::vec3(0.0f), kIdentity,
                 glm::vec3(2.0f * radius, 0.5f * length, 2.0f * radius), Solid::None);
            CapsuleCollider capsule;
            capsule.radius = radius;
            capsule.height = length;
            link.Add<CapsuleCollider>(capsule);
            RigidBody body;
            body.mass = 2.0f;
            body.angularDamping = 0.3f;
            link.Add<RigidBody>(body);
            Joint joint;
            joint.type = JointType::Point;
            joint.toParent = i > 0; // the first to the world
            JointAt(scene, link, glm::vec3(0.0f, 0.5f * length, 0.0f), kIdentity, joint);
            above = link;
        }
        return root;
    }

    Entity SpawnDoor(Scene& scene, AssetManager2& assets, const glm::vec3& position)
    {
        Entity root = scene.Instantiate("Door");
        root.SetPosition(position.x, position.y, position.z);
        const glm::vec3 wood(0.55f, 0.36f, 0.2f), frame(0.3f);
        Part(scene, assets, root, BuiltinMesh::Cube, "Post L", frame, glm::vec3(-0.1f, 1.1f, 0.0f), kIdentity, glm::vec3(0.15f, 2.2f, 0.2f));
        Part(scene, assets, root, BuiltinMesh::Cube, "Post R", frame, glm::vec3(1.3f, 1.1f, 0.0f), kIdentity, glm::vec3(0.15f, 2.2f, 0.2f));
        Part(scene, assets, root, BuiltinMesh::Cube, "Lintel", frame, glm::vec3(0.6f, 2.25f, 0.0f), kIdentity, glm::vec3(1.55f, 0.1f, 0.2f));
        Entity door = Part(scene, assets, root, BuiltinMesh::Cube, "Leaf", wood, glm::vec3(0.6f, 1.05f, 0.0f), kIdentity, glm::vec3(1.1f, 2.0f, 0.06f));
        RigidBody body;
        body.mass = 20.0f;
        door.Add<RigidBody>(body);
        Joint hinge;
        hinge.type = JointType::Hinge;
        hinge.limited = true;
        hinge.minLimit = -100.0f;
        hinge.maxLimit = 100.0f;
        hinge.friction = 2.0f;
        // On the leaf's left edge, its X axis up (the leaf is scaled, so in its space).
        JointAt(scene, door, glm::vec3(-0.55f / 1.1f * 1.0f, 0.0f, 0.0f), kXUp, hinge);
        return root;
    }

    Entity SpawnSpinner(Scene& scene, AssetManager2& assets, const glm::vec3& position)
    {
        Entity root = scene.Instantiate("Spinner");
        root.SetPosition(position.x, position.y, position.z);
        Entity table = Part(scene, assets, root, BuiltinMesh::Cylinder, "Turntable", glm::vec3(0.25f, 0.45f, 0.6f), glm::vec3(0.0f, 0.15f, 0.0f), kIdentity, glm::vec3(5.0f, 0.2f, 5.0f), Solid::Cylinder);
        RigidBody body;
        body.mass = 400.0f;
        table.Add<RigidBody>(body);
        Joint hinge;
        hinge.type = JointType::Hinge;
        hinge.motor = JointMotor::Velocity;
        hinge.motorSpeed = 60.0f;
        hinge.motorStrength = 20000.0f;
        JointAt(scene, table, glm::vec3(0.0f), kXUp, hinge);
        for (int i = 0; i < 4; ++i)
        {
            const float angle = glm::radians(90.0f * static_cast<float>(i));
            Entity crate = Part(scene, assets, root, BuiltinMesh::Cube, "Crate", glm::vec3(0.85f, 0.45f, 0.15f),
                                glm::vec3(1.6f * std::cos(angle), 0.65f, 1.6f * std::sin(angle)), kIdentity, glm::vec3(0.6f));
            RigidBody b;
            b.mass = 15.0f;
            crate.Add<RigidBody>(b);
        }
        return root;
    }

    Entity SpawnWreckingBall(Scene& scene, AssetManager2& assets, const glm::vec3& position)
    {
        Entity root = scene.Instantiate("Wrecking Ball");
        root.SetPosition(position.x, position.y, position.z);
        const float height = 7.0f, rope = 5.5f, pull = 3.5f;
        Part(scene, assets, root, BuiltinMesh::Cube, "Gantry", glm::vec3(0.3f), glm::vec3(0.0f, height + 0.15f, 0.0f), kIdentity, glm::vec3(0.4f, 0.3f, 0.4f), Solid::None);
        // The ball starts pulled aside along -X, the rope taut.
        const float drop = std::sqrt(rope * rope - pull * pull);
        Entity ball = Part(scene, assets, root, BuiltinMesh::Sphere, "Ball", glm::vec3(0.12f), glm::vec3(-pull, height - drop, 0.0f), kIdentity, glm::vec3(1.2f), Solid::Sphere);
        RigidBody body;
        body.mass = 500.0f;
        body.angularDamping = 0.2f;
        ball.Add<RigidBody>(body);
        Joint rope_;
        rope_.type = JointType::Distance;
        rope_.connectedAnchor = position + glm::vec3(0.0f, height, 0.0f); // a world point
        rope_.minDistance = 0.0f;
        rope_.maxDistance = rope;
        JointAt(scene, ball, glm::vec3(0.0f), kIdentity, rope_);
        // Crates in its way.
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
            {
                Entity crate = Part(scene, assets, root, BuiltinMesh::Cube, "Crate", glm::vec3(0.85f, 0.45f, 0.15f),
                                    glm::vec3(1.2f, 0.4f + 0.81f * static_cast<float>(row), -0.82f + 0.82f * static_cast<float>(column)), kIdentity, glm::vec3(0.8f));
                RigidBody b;
                b.mass = 25.0f;
                crate.Add<RigidBody>(b);
            }
        return root;
    }
}
