#include <Mariana/Physics/VehiclePresets.hpp>

#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Physics/Controllers.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>

#include <cmath>
#include <glm/gtc/constants.hpp>

namespace
{
    enum class Solid { None, Box, Cylinder, Sphere };

    Entity Place(Entity e, Entity parent, const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
    {
        if (parent.IsValid())
            e.SetParent(parent);
        e.SetPosition(position.x, position.y, position.z);
        e.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
        e.SetScale(scale.x, scale.y, scale.z);
        return e;
    }

    // A primitive below `parent`, optionally with a collider fitted to it.
    Entity Part(Scene& scene, AssetManager2& assets, Entity parent, BuiltinMesh shape, const char* name, const glm::vec3& color,
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

    glm::quat Axis(float degrees, const glm::vec3& axis) { return glm::angleAxis(glm::radians(degrees), axis); }
    const glm::quat kIdentity(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 kX(1, 0, 0), kY(0, 1, 0), kZ(0, 0, 1);

    // A wheel entity hanging at `rest` (suspension fully out), with a tyre lying along the axle
    // and a spoke across it.
    Entity Wheel(Scene& scene, AssetManager2& assets, Entity chassis, const char* name, const glm::vec3& rest, const VehicleWheel& settings,
                 const glm::vec3& tyreColor = glm::vec3(0.06f), float meshWidth = 0.0f)
    {
        Entity wheel = scene.Instantiate(name);
        Place(wheel, chassis, rest, kIdentity, glm::vec3(1.0f));
        wheel.Add<VehicleWheel>(settings);
        const float d = 2.0f * settings.radius;
        const float w = meshWidth > 0.0f ? meshWidth : settings.width;
        Part(scene, assets, wheel, BuiltinMesh::Cylinder, "Tyre", tyreColor, glm::vec3(0.0f), Axis(90.0f, kZ), glm::vec3(d, w, d), Solid::None);
        Part(scene, assets, wheel, BuiltinMesh::Cube, "Spoke", glm::vec3(0.75f), glm::vec3(0.0f), kIdentity, glm::vec3(w * 1.06f, d * 0.8f, d * 0.14f), Solid::None);
        return wheel;
    }

    Entity Car(Scene& scene, AssetManager2& assets)
    {
        Entity car = scene.Instantiate("Car");
        RigidBody body;
        body.mass = 1500.0f;
        body.centerOfMass = glm::vec3(0.0f, -0.35f, 0.0f);
        car.Add<RigidBody>(body);
        Vehicle vehicle;
        vehicle.type = VehicleType::Wheeled;
        vehicle.drive = VehicleDrive::Rear;
        car.Add<Vehicle>(vehicle);
        car.Add<PlayerControl>(PlayerControl{});

        Part(scene, assets, car, BuiltinMesh::Cube, "Body", glm::vec3(0.75f, 0.08f, 0.06f), glm::vec3(0.0f), kIdentity, glm::vec3(1.8f, 0.5f, 4.2f));
        Part(scene, assets, car, BuiltinMesh::Cube, "Cabin", glm::vec3(0.12f, 0.16f, 0.22f), glm::vec3(0.0f, 0.47f, -0.3f), kIdentity, glm::vec3(1.5f, 0.45f, 2.0f));
        Part(scene, assets, car, BuiltinMesh::Cube, "Lights", glm::vec3(1.0f, 0.95f, 0.7f), glm::vec3(0.0f, 0.05f, 2.11f), kIdentity, glm::vec3(1.4f, 0.12f, 0.04f), Solid::None);

        VehicleWheel front;
        front.radius = 0.38f;
        front.width = 0.26f;
        front.suspensionMin = 0.1f;
        front.suspensionMax = 0.3f;
        front.maxSteerAngle = 30.0f;
        VehicleWheel rear = front;
        rear.maxSteerAngle = 0.0f;
        rear.maxHandBrakeTorque = 4000.0f;
        const float x = 0.97f, y = -0.4f, z = 1.35f;
        Wheel(scene, assets, car, "Wheel FL", glm::vec3(-x, y, z), front);
        Wheel(scene, assets, car, "Wheel FR", glm::vec3(x, y, z), front);
        Wheel(scene, assets, car, "Wheel RL", glm::vec3(-x, y, -z), rear);
        Wheel(scene, assets, car, "Wheel RR", glm::vec3(x, y, -z), rear);
        return car;
    }

    // After Jolt's motorcycle sample (a Yamaha XJ 900).
    Entity Motorcycle(Scene& scene, AssetManager2& assets)
    {
        Entity bike = scene.Instantiate("Motorcycle");
        RigidBody body;
        body.mass = 240.0f;
        body.centerOfMass = glm::vec3(0.0f, -0.3f, 0.0f);
        bike.Add<RigidBody>(body);
        Vehicle vehicle;
        vehicle.type = VehicleType::Motorcycle;
        vehicle.engineTorque = 150.0f;
        vehicle.maxRPM = 10000.0f;
        vehicle.shiftUpRPM = 8000.0f;
        vehicle.shiftDownRPM = 2000.0f;
        vehicle.gearRatios = { 2.27f, 1.63f, 1.3f, 1.09f, 0.96f, 0.88f };
        vehicle.reverseRatio = -4.0f;
        vehicle.clutchStrength = 2.0f;
        vehicle.differentialRatio = 1.93f * 40.0f / 16.0f;
        vehicle.antiRollStiffness = 0.0f;
        bike.Add<Vehicle>(vehicle);
        bike.Add<PlayerControl>(PlayerControl{});

        Part(scene, assets, bike, BuiltinMesh::Cube, "Frame", glm::vec3(0.08f, 0.2f, 0.65f), glm::vec3(0.0f), kIdentity, glm::vec3(0.4f, 0.6f, 0.8f));
        Part(scene, assets, bike, BuiltinMesh::Cube, "Seat", glm::vec3(0.05f), glm::vec3(0.0f, 0.35f, -0.2f), kIdentity, glm::vec3(0.3f, 0.1f, 0.55f), Solid::None);
        Part(scene, assets, bike, BuiltinMesh::Cube, "Handlebar", glm::vec3(0.6f), glm::vec3(0.0f, 0.5f, 0.42f), kIdentity, glm::vec3(0.7f, 0.05f, 0.05f), Solid::None);

        VehicleWheel front;
        front.radius = 0.31f;
        front.width = 0.05f;
        front.suspensionMin = 0.3f;
        front.suspensionMax = 0.5f;
        // Stiffer than Jolt's sample, whose light bike sits on its bump stops.
        front.suspensionFrequency = 3.0f;
        front.maxSteerAngle = 30.0f;
        front.casterAngle = 30.0f;
        front.maxBrakeTorque = 500.0f;
        VehicleWheel back = front;
        back.suspensionFrequency = 3.5f;
        back.maxSteerAngle = 0.0f;
        back.casterAngle = 0.0f;
        back.maxBrakeTorque = 250.0f;
        // Hung where the suspension, from its attachment, is fully out.
        const glm::vec3 frontDirection = glm::normalize(glm::vec3(0.0f, -1.0f, std::tan(glm::radians(front.casterAngle))));
        Wheel(scene, assets, bike, "Front Wheel", glm::vec3(0.0f, -0.27f, 0.75f) + frontDirection * front.suspensionMax, front, glm::vec3(0.06f), 0.12f);
        Wheel(scene, assets, bike, "Back Wheel", glm::vec3(0.0f, -0.27f - back.suspensionMax, -0.75f), back, glm::vec3(0.06f), 0.14f);
        return bike;
    }

    // After Jolt's tank sample, the turret fixed.
    Entity Tank(Scene& scene, AssetManager2& assets)
    {
        Entity tank = scene.Instantiate("Tank");
        RigidBody body;
        body.mass = 4000.0f;
        body.centerOfMass = glm::vec3(0.0f, -0.5f, 0.0f);
        tank.Add<RigidBody>(body);
        Vehicle vehicle;
        vehicle.type = VehicleType::Tracked;
        vehicle.collision = WheelCollision::Ray;
        vehicle.differentialRatio = 6.0f;
        tank.Add<Vehicle>(vehicle);
        tank.Add<PlayerControl>(PlayerControl{});

        const glm::vec3 olive(0.28f, 0.32f, 0.17f), darkOlive(0.2f, 0.24f, 0.12f);
        Part(scene, assets, tank, BuiltinMesh::Cube, "Hull", olive, glm::vec3(0.0f), kIdentity, glm::vec3(3.4f, 1.0f, 6.4f));
        Part(scene, assets, tank, BuiltinMesh::Cube, "Turret", darkOlive, glm::vec3(0.0f, 0.9f, -0.2f), kIdentity, glm::vec3(2.8f, 0.8f, 3.6f));
        Part(scene, assets, tank, BuiltinMesh::Cylinder, "Barrel", darkOlive, glm::vec3(0.0f, 0.95f, 3.1f), Axis(90.0f, kX), glm::vec3(0.22f, 3.0f, 0.22f), Solid::Cylinder);
        Part(scene, assets, tank, BuiltinMesh::Cube, "Track L", glm::vec3(0.07f), glm::vec3(-1.85f, -0.55f, 0.0f), kIdentity, glm::vec3(0.5f, 0.7f, 6.3f), Solid::None);
        Part(scene, assets, tank, BuiltinMesh::Cube, "Track R", glm::vec3(0.07f), glm::vec3(1.85f, -0.55f, 0.0f), kIdentity, glm::vec3(0.5f, 0.7f, 6.3f), Solid::None);

        const glm::vec3 attachments[] = {
            { 0.0f, 0.0f, 2.95f }, { 0.0f, -0.3f, 2.1f }, { 0.0f, -0.3f, 1.4f }, { 0.0f, -0.3f, 0.7f }, { 0.0f, -0.3f, 0.0f },
            { 0.0f, -0.3f, -0.7f }, { 0.0f, -0.3f, -1.4f }, { 0.0f, -0.3f, -2.1f }, { 0.0f, 0.0f, -2.75f },
        };
        const int count = static_cast<int>(std::size(attachments));
        for (int side = 0; side < 2; ++side)
        {
            for (int i = 0; i < count; ++i)
            {
                VehicleWheel w;
                w.radius = 0.3f;
                w.width = 0.1f;
                w.suspensionMin = 0.3f;
                w.suspensionMax = i == 0 || i == count - 1 ? 0.3f : 0.5f; // the end wheels hold the track up
                w.suspensionFrequency = 1.0f;
                w.maxBrakeTorque = 0.0f;
                glm::vec3 rest = attachments[i] + glm::vec3(0.0f, -w.suspensionMax, 0.0f);
                rest.x = side == 0 ? -1.85f : 1.85f;
                const std::string name = std::string(side == 0 ? "Wheel L" : "Wheel R") + std::to_string(i + 1);
                Wheel(scene, assets, tank, name.c_str(), rest, w, glm::vec3(0.12f), 0.55f);
            }
        }
        return tank;
    }
}

namespace VehiclePresets
{
    Entity Spawn(Scene& scene, AssetManager2& assets, VehicleType type, const glm::vec3& position, const glm::quat& rotation)
    {
        Entity vehicle = type == VehicleType::Motorcycle ? Motorcycle(scene, assets) : type == VehicleType::Tracked ? Tank(scene, assets) : Car(scene, assets);
        vehicle.SetPosition(position.x, position.y, position.z);
        vehicle.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
        return vehicle;
    }

    Entity SpawnPlayground(Scene& scene, AssetManager2& assets, const glm::vec3& center)
    {
        Entity root = scene.Instantiate("Vehicle Playground");
        root.SetPosition(center.x, center.y, center.z);
        const glm::vec3 grass(0.32f, 0.4f, 0.27f), concrete(0.55f, 0.55f, 0.52f), orange(0.85f, 0.45f, 0.15f);

        Part(scene, assets, root, BuiltinMesh::Plane, "Ground", grass, glm::vec3(0.0f), kIdentity, glm::vec3(20.0f));

        // A tabletop: up a ramp, across a solid table, down the other side (tilted boxes overhang at
        // their high ends, so the table fills in under them).
        const float rampAngle = 12.0f, rampLength = 12.0f, tableLength = 10.0f;
        const float s = std::sin(glm::radians(rampAngle)), c = std::cos(glm::radians(rampAngle));
        const float rampY = 0.5f * rampLength * s - 0.1f;
        const float top = rampY + 0.5f * rampLength * s + 0.2f * c; // the ramps' surface at their high ends
        const float upEnd = 30.0f + 0.5f * rampLength * c;
        Part(scene, assets, root, BuiltinMesh::Cube, "Ramp Up", concrete, glm::vec3(0.0f, rampY, 30.0f), Axis(-rampAngle, kX), glm::vec3(7.0f, 0.4f, rampLength));
        Part(scene, assets, root, BuiltinMesh::Cube, "Table", concrete, glm::vec3(0.0f, 0.5f * top, upEnd + 0.5f * tableLength), kIdentity, glm::vec3(7.0f, top, tableLength));
        Part(scene, assets, root, BuiltinMesh::Cube, "Ramp Down", concrete, glm::vec3(0.0f, rampY, upEnd + tableLength + 0.5f * rampLength * c), Axis(rampAngle, kX),
             glm::vec3(7.0f, 0.4f, rampLength));

        // A gentle wide ramp to the side, and a kicker.
        Part(scene, assets, root, BuiltinMesh::Cube, "Wide Ramp", concrete, glm::vec3(-25.0f, 0.6f, 10.0f), Axis(-6.0f, kX), glm::vec3(12.0f, 0.4f, 16.0f));
        Part(scene, assets, root, BuiltinMesh::Cube, "Kicker", orange, glm::vec3(18.0f, 0.35f, 20.0f), Axis(-20.0f, kX), glm::vec3(4.0f, 0.3f, 2.5f));

        // Bumps to drive over (logs half in the ground).
        for (int i = 0; i < 6; ++i)
            Part(scene, assets, root, BuiltinMesh::Cylinder, "Bump", glm::vec3(0.4f, 0.3f, 0.2f), glm::vec3(0.0f, 0.0f, -18.0f - 3.0f * static_cast<float>(i)), Axis(90.0f, kZ),
                 glm::vec3(0.35f, 8.0f, 0.35f), Solid::Cylinder);

        // A banked turn.
        for (int i = 0; i < 5; ++i)
        {
            const float angle = -60.0f + 30.0f * static_cast<float>(i);
            const glm::quat facing = Axis(angle, kY);
            const glm::vec3 at = glm::vec3(45.0f, 0.0f, 10.0f) + facing * glm::vec3(0.0f, 0.0f, -14.0f);
            Part(scene, assets, root, BuiltinMesh::Cube, "Bank", concrete, at + glm::vec3(0.0f, 0.7f, 0.0f), facing * Axis(18.0f, kX), glm::vec3(9.0f, 0.4f, 6.0f));
        }

        // A wall of light boxes to drive through.
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                Entity box = Part(scene, assets, root, BuiltinMesh::Cube, "Crate", orange, glm::vec3(-20.0f + 0.82f * static_cast<float>(column) - 1.2f, 0.4f + 0.81f * static_cast<float>(row), 28.0f),
                                  kIdentity, glm::vec3(0.8f));
                RigidBody b;
                b.mass = 20.0f;
                box.Add<RigidBody>(b);
            }
        }
        return root;
    }
}
