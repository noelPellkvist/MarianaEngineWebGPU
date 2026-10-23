#pragma once
// Physics scenarios with known answers, run by the native smoke test (MarianaPhysicsTest, Jolt's
// thread pool) and by the web build (single-threaded), so both are checked on the same numbers.
// Colliders only: no window, GPU or meshes. Every scenario has a scene and a world of its own and
// steps exactly 1/60 s at a time.
#include <Mariana/Physics/Character.hpp>
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Physics/Joint.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Physics/Ragdoll.hpp>
#include <Mariana/Physics/Vehicle.hpp>
#include <Mariana/Physics/Water.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace PhysicsScenarios
{
    struct Result
    {
        std::string name;
        std::string detail;
        bool passed = false;
    };

    using Log = std::function<void(const std::string&)>;

    namespace detail
    {
        constexpr float kStep = 1.0f / 60.0f;

        inline std::string Format(const char* format, double a = 0, double b = 0, double c = 0, double d = 0)
        {
            char text[256];
            std::snprintf(text, sizeof(text), format, a, b, c, d);
            return text;
        }

        inline PhysicsSettings Settings()
        {
            PhysicsSettings settings;
            settings.interpolation = false; // read where the steps leave the bodies
            return settings;
        }

        inline glm::vec3 Position(Entity e)
        {
            const LocalTRS* t = e.TryGet<LocalTRS>();
            return t ? glm::vec3(t->pos[0], t->pos[1], t->pos[2]) : glm::vec3(0.0f);
        }

        inline Entity Make(Scene& scene, const char* name, const glm::vec3& position, Entity parent = {})
        {
            Entity e = scene.Instantiate(name);
            if (parent.IsValid())
                e.SetParent(parent);
            e.SetPosition(position.x, position.y, position.z);
            return e;
        }

        inline Entity Box(Scene& scene, const char* name, const glm::vec3& position, const glm::vec3& size, MotionType motion, float mass)
        {
            Entity e = Make(scene, name, position);
            BoxCollider box;
            box.size = size;
            e.Add<BoxCollider>(box);
            if (motion != MotionType::Static)
            {
                RigidBody body;
                body.motion = motion;
                body.mass = mass;
                e.Add<RigidBody>(body);
            }
            return e;
        }

        inline void Run(Scene& scene, PhysicsWorld& world, int steps, const std::function<void(int)>& each = {})
        {
            for (int i = 0; i < steps; ++i)
            {
                if (each)
                    each(i);
                world.Step(kStep);
                scene.Update(0.0f);
            }
        }

        inline Result Finish(const Log& log, const char* name, std::string detail, bool passed)
        {
            log(std::string(passed ? "  ok    " : "  FAIL  ") + name + ": " + detail);
            return Result{ name, std::move(detail), passed };
        }
    }

    // Ten 1 m boxes stacked on the ground: after 4 s they stand where they were put, sunk by at most
    // the penetration slop per contact.
    inline Result Stack(const Log& log)
    {
        using namespace detail;
        Scene scene;
        scene.UpdateComponentRegistry();
        PhysicsWorld world(scene, Settings());
        Box(scene, "Ground", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f), MotionType::Static, 0.0f);
        std::vector<Entity> boxes;
        for (int i = 0; i < 10; ++i)
            boxes.push_back(Box(scene, "Box", glm::vec3(0.0f, 0.5f + 1.01f * i, 0.0f), glm::vec3(1.0f), MotionType::Dynamic, 10.0f));
        Run(scene, world, 240);
        float drift = 0.0f;
        for (Entity b : boxes)
            drift = std::max(drift, glm::length(glm::vec2(Position(b).x, Position(b).z)));
        const float top = Position(boxes.back()).y;
        const float sunk = 9.5f - top;
        return Finish(log, "stack", Format("top box at y %.4f (sunk %.4f m over 10 contacts), drift %.4f m", top, sunk, drift),
                      sunk > -0.001f && sunk < 10.0f * world.Settings().penetrationSlop + 0.01f && drift < 0.02f);
    }

    // A 1 m ball dropped on the ground rests half a meter up, less the penetration slop.
    inline Result Ball(const Log& log)
    {
        using namespace detail;
        Scene scene;
        scene.UpdateComponentRegistry();
        PhysicsWorld world(scene, Settings());
        Box(scene, "Ground", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f), MotionType::Static, 0.0f);
        Entity ball = Make(scene, "Ball", glm::vec3(0.0f, 2.0f, 0.0f));
        ball.Add<SphereCollider>(SphereCollider{});
        ball.Add<RigidBody>(RigidBody{});
        Run(scene, world, 180);
        const float y = Position(ball).y;
        return Finish(log, "ball", Format("rests at y %.4f (slop %.3f)", y, world.Settings().penetrationSlop),
                      y > 0.5f - world.Settings().penetrationSlop - 0.002f && y < 0.5001f);
    }

    // A small ball on a ball joint 2 m below its pivot, let go at 10 degrees: its period is that of
    // a physical pendulum, 2 pi sqrt((L^2 + 2/5 r^2) / (g L)), times 1 + amplitude^2 / 16.
    inline Result Pendulum(const Log& log)
    {
        using namespace detail;
        Scene scene;
        scene.UpdateComponentRegistry();
        PhysicsWorld world(scene, Settings());
        const float length = 2.0f, radius = 0.1f, angle = glm::radians(10.0f);
        const glm::vec3 pivot(0.0f, 3.0f, 0.0f);
        const glm::vec3 start = pivot + glm::vec3(std::sin(angle), -std::cos(angle), 0.0f) * length;
        Entity bob = Make(scene, "Bob", start);
        SphereCollider sphere;
        sphere.radius = radius;
        bob.Add<SphereCollider>(sphere);
        RigidBody body;
        body.linearDamping = 0.0f;
        body.angularDamping = 0.0f;
        body.allowSleeping = false;
        bob.Add<RigidBody>(body);
        Entity pin = Make(scene, "Pivot", pivot - start, bob);
        Joint joint;
        joint.type = JointType::Point;
        pin.Add<Joint>(joint);

        // Times it swings back through the bottom, from the right.
        std::vector<float> crossings;
        float lastX = Position(bob).x;
        Run(scene, world, 60 * 12, [&](int step) {
            const float x = Position(bob).x;
            if (lastX > 0.0f && x <= 0.0f)
                crossings.push_back((step - 1 + lastX / (lastX - x)) * kStep); // between the two samples
            lastX = x;
        });
        const float g = 9.81f;
        const float expected = glm::two_pi<float>() * std::sqrt((length * length + 0.4f * radius * radius) / (g * length)) * (1.0f + angle * angle / 16.0f);
        float period = 0.0f;
        if (crossings.size() >= 2)
            period = (crossings.back() - crossings.front()) / static_cast<float>(crossings.size() - 1);
        return Finish(log, "pendulum", Format("period %.4f s over %.0f swings (expected %.4f)", period, crossings.size() > 0 ? crossings.size() - 1.0 : 0.0, expected),
                      std::abs(period - expected) < 0.01f * expected);
    }

    // The primitive car of VehiclePresets (its colliders only) on full throttle for 5 s: about 74 km/h
    // in 2nd gear.
    inline Result Car(const Log& log)
    {
        using namespace detail;
        Scene scene;
        scene.UpdateComponentRegistry();
        PhysicsWorld world(scene, Settings());
        Box(scene, "Ground", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(400.0f, 1.0f, 400.0f), MotionType::Static, 0.0f);
        Entity car = Make(scene, "Car", glm::vec3(0.0f, 1.0f, -150.0f));
        RigidBody body;
        body.mass = 1500.0f;
        body.centerOfMass = glm::vec3(0.0f, -0.35f, 0.0f);
        car.Add<RigidBody>(body);
        Vehicle vehicle;
        vehicle.drive = VehicleDrive::Rear;
        car.Add<Vehicle>(vehicle);
        Entity chassis = Make(scene, "Body", glm::vec3(0.0f), car);
        chassis.SetScale(1.8f, 0.5f, 4.2f);
        chassis.Add<BoxCollider>(BoxCollider{});
        Entity cabin = Make(scene, "Cabin", glm::vec3(0.0f, 0.47f, -0.3f), car);
        cabin.SetScale(1.5f, 0.45f, 2.0f);
        cabin.Add<BoxCollider>(BoxCollider{});
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
        Make(scene, "Wheel FL", glm::vec3(-x, y, z), car).Add<VehicleWheel>(front);
        Make(scene, "Wheel FR", glm::vec3(x, y, z), car).Add<VehicleWheel>(front);
        Make(scene, "Wheel RL", glm::vec3(-x, y, -z), car).Add<VehicleWheel>(rear);
        Make(scene, "Wheel RR", glm::vec3(x, y, -z), car).Add<VehicleWheel>(rear);

        Run(scene, world, 60); // settle
        car.Get<Vehicle>()->throttle = 1.0f;
        Run(scene, world, 300);
        VehicleInfo info;
        world.GetVehicleInfo(car, info);
        const float kmh = info.speed * 3.6f;
        return Finish(log, "car", Format("%.1f km/h after 5 s in gear %.0f, %.0f rpm", kmh, info.gear, info.rpm),
                      kmh > 65.0f && kmh < 85.0f && info.gear >= 2);
    }

    // A character walking up 8 stairs of 0.25 m (for 1.75 s, then standing) is on the landing at 2 m.
    inline Result Stairs(const Log& log)
    {
        using namespace detail;
        Scene scene;
        scene.UpdateComponentRegistry();
        PhysicsWorld world(scene, Settings());
        Box(scene, "Ground", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f), MotionType::Static, 0.0f);
        for (int i = 0; i < 8; ++i)
        {
            const float top = 0.25f * (i + 1);
            Box(scene, "Stair", glm::vec3(0.0f, 0.5f * top, 2.0f + 0.4f * i + 0.2f), glm::vec3(3.0f, top, 0.4f), MotionType::Static, 0.0f);
        }
        Box(scene, "Landing", glm::vec3(0.0f, 1.0f, 2.0f + 3.2f + 2.0f), glm::vec3(3.0f, 2.0f, 4.0f), MotionType::Static, 0.0f);
        Entity walker = Make(scene, "Character", glm::vec3(0.0f, 0.0f, 0.0f));
        walker.Add<CharacterController>(CharacterController{});
        Run(scene, world, 360, [&](int step) { walker.Get<CharacterController>()->move = glm::vec3(0.0f, 0.0f, step < 105 ? 1.0f : 0.0f); });
        const glm::vec3 p = Position(walker);
        return Finish(log, "stairs", Format("at y %.3f, z %.2f", p.y, p.z), std::abs(p.y - 2.0f) < 0.05f && p.z > 5.5f && p.z < 9.0f);
    }

    // Water 4 m deep: a 0.5 m cube of 500 kg/m^3 floats half under (its center at the surface), one
    // of 2000 kg/m^3 sinks to the bottom.
    inline Result Water(const Log& log)
    {
        using namespace detail;
        Scene scene;
        scene.UpdateComponentRegistry();
        PhysicsWorld world(scene, Settings());
        Box(scene, "Ground", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f), MotionType::Static, 0.0f);
        Entity water = Make(scene, "Water", glm::vec3(0.0f, 2.0f, 0.0f));
        WaterVolume volume;
        volume.size = glm::vec3(10.0f, 4.0f, 10.0f);
        water.Add<WaterVolume>(volume);
        auto cube = [&](const char* name, float x, float density) {
            Entity e = Make(scene, name, glm::vec3(x, 4.6f, 0.0f));
            BoxCollider box;
            box.size = glm::vec3(0.5f);
            box.density = density;
            e.Add<BoxCollider>(box);
            RigidBody body;
            body.mass = 0.0f; // from the density
            e.Add<RigidBody>(body);
            return e;
        };
        Entity wood = cube("Wood", -2.0f, 500.0f);
        Entity stone = cube("Stone", 2.0f, 2000.0f);
        Run(scene, world, 60 * 10);
        const float floating = Position(wood).y, sunk = Position(stone).y;
        return Finish(log, "water", Format("wood's center at y %.4f (surface 4), stone at y %.4f (bottom 0.25)", floating, sunk),
                      std::abs(floating - 4.0f) < 0.01f && std::abs(sunk - 0.25f) < 0.02f);
    }

    // A limp ragdoll of a 1.8 m skeleton (Mixamo's bone names, a T-pose, no skin) dropped from 2 m
    // comes to rest on the ground: nothing below it, nothing still standing.
    inline Result RagdollFall(const Log& log)
    {
        using namespace detail;
        Scene scene;
        scene.UpdateComponentRegistry();
        PhysicsWorld world(scene, Settings());
        Box(scene, "Ground", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f), MotionType::Static, 0.0f);
        Entity model = Make(scene, "Model", glm::vec3(0.0f));
        Ragdoll ragdoll;
        ragdoll.mode = RagdollMode::Limp;
        model.Add<Ragdoll>(ragdoll);
        std::vector<Entity> bones;
        auto bone = [&](const char* name, Entity parent, glm::vec3 at) { Entity e = Make(scene, name, at, parent); bones.push_back(e); return e; };
        Entity hips = bone("mixamorig:Hips", model, glm::vec3(0.0f, 3.0f, 0.0f));
        Entity spine = bone("mixamorig:Spine", hips, glm::vec3(0.0f, 0.12f, 0.0f));
        Entity spine1 = bone("mixamorig:Spine1", spine, glm::vec3(0.0f, 0.14f, 0.0f));
        Entity spine2 = bone("mixamorig:Spine2", spine1, glm::vec3(0.0f, 0.14f, 0.0f));
        Entity neck = bone("mixamorig:Neck", spine2, glm::vec3(0.0f, 0.16f, 0.0f));
        Entity head = bone("mixamorig:Head", neck, glm::vec3(0.0f, 0.1f, 0.0f));
        bone("mixamorig:HeadTop_End", head, glm::vec3(0.0f, 0.2f, 0.0f));
        for (float side : { 1.0f, -1.0f })
        {
            const bool left = side > 0.0f;
            Entity shoulder = bone(left ? "mixamorig:LeftShoulder" : "mixamorig:RightShoulder", spine2, glm::vec3(0.05f * side, 0.1f, 0.0f));
            Entity arm = bone(left ? "mixamorig:LeftArm" : "mixamorig:RightArm", shoulder, glm::vec3(0.12f * side, 0.0f, 0.0f));
            Entity forearm = bone(left ? "mixamorig:LeftForeArm" : "mixamorig:RightForeArm", arm, glm::vec3(0.28f * side, 0.0f, 0.0f));
            Entity hand = bone(left ? "mixamorig:LeftHand" : "mixamorig:RightHand", forearm, glm::vec3(0.26f * side, 0.0f, 0.0f));
            bone(left ? "mixamorig:LeftHandMiddle1" : "mixamorig:RightHandMiddle1", hand, glm::vec3(0.1f * side, 0.0f, 0.0f));
            Entity thigh = bone(left ? "mixamorig:LeftUpLeg" : "mixamorig:RightUpLeg", hips, glm::vec3(0.1f * side, -0.06f, 0.0f));
            Entity shin = bone(left ? "mixamorig:LeftLeg" : "mixamorig:RightLeg", thigh, glm::vec3(0.0f, -0.42f, 0.0f));
            Entity foot = bone(left ? "mixamorig:LeftFoot" : "mixamorig:RightFoot", shin, glm::vec3(0.0f, -0.42f, 0.0f));
            bone(left ? "mixamorig:LeftToeBase" : "mixamorig:RightToeBase", foot, glm::vec3(0.0f, -0.06f, 0.14f));
        }
        scene.Update(0.0f);
        const size_t planned = PlanRagdoll(scene, model).size();
        Run(scene, world, 60 * 5);
        RagdollInfo info;
        const bool made = world.GetRagdollInfo(model, info);
        float lowest = 1e9f, highest = -1e9f;
        for (const RagdollInfo::Bone& b : info.bones)
        {
            lowest = std::min({ lowest, b.a.y - b.radius, b.b.y - b.radius });
            highest = std::max({ highest, b.a.y + b.radius, b.b.y + b.radius });
        }
        return Finish(log, "ragdoll", Format("%.0f bodies (of %.0f bones); after 5 s its lowest point at y %.3f, highest %.3f", static_cast<double>(info.bones.size()), static_cast<double>(bones.size()), lowest, highest),
                      made && planned == 18 && info.bones.size() == 18 && lowest > -0.02f && highest < 0.6f);
    }

    inline std::vector<Result> RunAll(const Log& log)
    {
        std::vector<Result> results;
        for (auto scenario : { &Stack, &Ball, &Pendulum, &Car, &Stairs, &Water, &RagdollFall })
            results.push_back(scenario(log));
        return results;
    }
}
