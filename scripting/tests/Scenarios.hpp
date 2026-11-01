#pragma once
// Script scenarios with known answers, run by the native test (MarianaScriptingTest) and by the web
// build, so both are checked on the same numbers. No window or GPU: scripts given as source or the
// Sandbox project's (its folder must be a FileSystem root), scenes made in code, physics on
// colliders, keys simulated. Scripts report what they saw in their entity's name.
#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/GameClock.hpp>
#include <Mariana/Physics/Components.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Physics/Character.hpp>
#include <Mariana/Physics/Vehicle.hpp>
#include <Mariana/Platform/IInput.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Scene/Transform.hpp>
#include <Mariana/Scripting/ScriptWorld.hpp>

#include "../../engine/tests/TestRig.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace ScriptScenarios
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

        // A scene, its scripts, physics, clock and keys, stepped a frame at a time as the apps do.
        struct World
        {
            AssetManager2 assets;
            Scene scene;
            GameClock clock;
            IInput input{ nullptr };
            PhysicsWorld physics;
            ScriptWorld scripts;
            System animation; // queries the scene: made after it, so destroyed first

            World() : physics(scene, Settings()), scripts(scene, assets, clock)
            {
                scene.UpdateComponentRegistry();
                ScriptAsset::RegisterLoader(assets);
                scripts.SetPhysics(&physics);
                animation = Animator::CreateSystem(scene);
                clock.Play();
            }

            static PhysicsSettings Settings()
            {
                PhysicsSettings settings;
                settings.interpolation = false;
                return settings;
            }

            void Frames(int count)
            {
                for (int i = 0; i < count; ++i)
                {
                    clock.Tick(kStep);
                    scripts.Update(kStep, &input);
                    animation.Run(kStep);
                    physics.Step(kStep);
                    scripts.LateUpdate(kStep);
                    scene.Update(kStep);
                    input.Update();
                }
            }

            AssetHandle<ScriptAsset> Source(const char* name, const char* code)
            {
                auto script = std::make_shared<ScriptAsset>();
                script->SetSource(code);
                return assets.RegisterAsset<ScriptAsset>(script, AssetType::Script, name);
            }

            Entity Make(const char* name, const glm::vec3& position = glm::vec3(0.0f), Entity parent = {})
            {
                Entity e = scene.Instantiate(name);
                if (parent.IsValid())
                    e.SetParent(parent);
                e.SetPosition(position.x, position.y, position.z);
                return e;
            }
        };

        inline void Attach(Entity e, const AssetHandle<ScriptAsset>& script, std::vector<ScriptValue> values = {})
        {
            if (!e.Has<Scripts>())
                e.Add<Scripts>(Scripts{});
            e.Get<Scripts>()->scripts.push_back(ScriptSlot{ script, true, std::move(values) });
        }

        inline ScriptValue Number(const char* name, float n)
        {
            ScriptValue v;
            v.name = name;
            v.number = n;
            return v;
        }

        inline ScriptValue Vector(const char* name, const glm::vec3& vector)
        {
            ScriptValue v;
            v.name = name;
            v.type = ScriptValueType::Vector;
            v.vector = vector;
            return v;
        }

        inline glm::vec3 Position(Entity e)
        {
            const LocalTRS* t = e.TryGet<LocalTRS>();
            return t ? glm::vec3(t->pos[0], t->pos[1], t->pos[2]) : glm::vec3(0.0f);
        }

        inline std::string Format(const char* format, double a = 0, double b = 0, double c = 0, double d = 0)
        {
            char text[256];
            std::snprintf(text, sizeof(text), format, a, b, c, d);
            return text;
        }

        // The primitive car of the physics test's "car" scenario (its colliders), on a 400 m ground.
        inline Entity PrimitiveCar(World& w)
        {
            Entity ground = w.Make("Ground", glm::vec3(0.0f, -0.5f, 0.0f));
            BoxCollider groundBox;
            groundBox.size = glm::vec3(400.0f, 1.0f, 400.0f);
            ground.Add<BoxCollider>(groundBox);
            Entity car = w.Make("Car", glm::vec3(0.0f, 1.0f, -150.0f));
            RigidBody body;
            body.mass = 1500.0f;
            body.centerOfMass = glm::vec3(0.0f, -0.35f, 0.0f);
            car.Add<RigidBody>(body);
            Vehicle vehicle;
            vehicle.drive = VehicleDrive::Rear;
            car.Add<Vehicle>(vehicle);
            Entity chassis = w.Make("Body", glm::vec3(0.0f), car);
            chassis.SetScale(1.8f, 0.5f, 4.2f);
            chassis.Add<BoxCollider>(BoxCollider{});
            Entity cabin = w.Make("Cabin", glm::vec3(0.0f, 0.47f, -0.3f), car);
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
            w.Make("Wheel FL", glm::vec3(-x, y, z), car).Add<VehicleWheel>(front);
            w.Make("Wheel FR", glm::vec3(x, y, z), car).Add<VehicleWheel>(front);
            w.Make("Wheel RL", glm::vec3(-x, y, -z), car).Add<VehicleWheel>(rear);
            w.Make("Wheel RR", glm::vec3(x, y, -z), car).Add<VehicleWheel>(rear);
            return car;
        }

        inline Result Finish(const Log& log, const char* name, std::string detail, bool passed)
        {
            log(std::string(passed ? "  ok    " : "  FAIL  ") + name + ": " + detail);
            return Result{ name, std::move(detail), passed };
        }

        inline const char* kMover = R"(
local Mover = {}
Mover.properties = { speed = 1, label = "mover", offset = vec3(0, 0, 0), active = true }

function Mover:Start()
    self.starts = (self.starts or 0) + 1
    self.updates = 0
    self.entity.position = self.offset
end

function Mover:Update(dt)
    self.updates += 1
    self.entity.position += vec3(self.speed * dt, 0, 0)
end

function Mover:LateUpdate()
    self.entity.name = `{self.label} starts={self.starts} updates={self.updates}`
end

return Mover
)";
    }

    // Start once, then Update every frame; a property's default and an entity's own value.
    inline Result Lifecycle(const Log& log)
    {
        using namespace detail;
        World w;
        const AssetHandle<ScriptAsset> mover = w.Source("Mover.luau", kMover);
        Entity a = w.Make("A");
        Entity b = w.Make("B");
        Attach(a, mover);
        Attach(b, mover, { Number("speed", 2.0f), Vector("offset", glm::vec3(0.0f, 5.0f, 0.0f)) });
        w.Frames(60);
        const glm::vec3 pa = Position(a), pb = Position(b);
        const std::string name = a.GetName();
        return Finish(log, "lifecycle", Format("a.x %.4f, b (%.4f, %.1f), ", pa.x, pb.x, pb.y) + name,
                      std::abs(pa.x - 1.0f) < 1e-3f && std::abs(pb.x - 2.0f) < 2e-3f && pb.y == 5.0f && name == "mover starts=1 updates=60");
    }

    // Components by name: added with fields, read and written (enums by name), errors for fields that
    // don't exist, removed.
    inline Result Components(const Log& log)
    {
        using namespace detail;
        World w;
        const AssetHandle<ScriptAsset> script = w.Source("Builder.luau", R"(
local B = {}
function B:Start()
    local e = self.entity
    local rb = e:Add("RigidBody", { motion = "Kinematic", mass = 5 })
    rb.mass += 2
    rb.motion = "Dynamic"
    local ok, err = pcall(function() rb.masss = 1 end)
    local box = e:Add("BoxCollider")
    box.size = vec3(2, 1, 1)
    local trs = e:Get("LocalTRS")
    trs.position = vec3(0, 3, 0)
    self.report = `{rb.motion} {rb.mass} {ok} {e:Has("BoxCollider")} {box.size.x} {string.find(err, "masss") ~= nil}`
end
function B:Update()
    if self.entity:Has("SphereCollider") then
        self.entity:Remove("SphereCollider")
    else
        self.entity:Add("SphereCollider", { radius = 0.25 })
    end
    self.entity.name = self.report
end
return B
)");
        Entity e = w.Make("E");
        Attach(e, script);
        w.Frames(1);
        const RigidBody* rb = e.TryGet<RigidBody>();
        const BoxCollider* box = e.TryGet<BoxCollider>();
        const bool sphereAfterOne = e.Has<SphereCollider>();
        const float radius = sphereAfterOne ? e.TryGet<SphereCollider>()->radius : 0.0f;
        w.Frames(1);
        const bool sphereAfterTwo = e.Has<SphereCollider>();
        const std::string name = e.GetName();
        const bool passed = rb && rb->motion == MotionType::Dynamic && rb->mass == 7.0f && box && box->size == glm::vec3(2.0f, 1.0f, 1.0f) &&
                            Position(e).y > 2.9f && sphereAfterOne && radius == 0.25f && !sphereAfterTwo && name == "Dynamic 7 false true 2 true";
        return Finish(log, "components", name + (passed ? "" : Format(" (sphere %.0f %.2f %.0f)", sphereAfterOne, radius, sphereAfterTwo)), passed);
    }

    // Contact callbacks, a raycast, and an impulse on a body the script made.
    inline Result Physics(const Log& log)
    {
        using namespace detail;
        World w;
        Entity ground = w.Make("Ground", glm::vec3(0.0f, -0.5f, 0.0f));
        BoxCollider groundBox;
        groundBox.size = glm::vec3(40.0f, 1.0f, 40.0f);
        ground.Add<BoxCollider>(groundBox);

        const AssetHandle<ScriptAsset> ball = w.Source("Ball.luau", R"(
local Ball = {}
function Ball:Start()
    self.contacts = 0
    self.entity:Add("SphereCollider", { radius = 0.5 })
    self.entity:Add("RigidBody", { motion = "Dynamic", mass = 1 })
end
function Ball:OnContactBegin(other, contact)
    self.contacts += 1
    self.other = other.name
    self.speed = contact.speed
    self.up = contact.normal.y
end
function Ball:LateUpdate()
    local hit = physics.Raycast(vec3(5, 10, 0), vec3(0, -1, 0))
    local ground = if hit then math.round(hit.point.y * 1000) / 1000 + 0 else -1
    self.entity.name = string.format("contacts=%d other=%s ground=%.3f up=%.0f|%.3f", self.contacts, tostring(self.other),
        ground, self.up or 0, self.speed or 0)
end
return Ball
)");
        const AssetHandle<ScriptAsset> pushed = w.Source("Pushed.luau", R"(
local P = {}
function P:Start()
    self.entity:Add("SphereCollider", { radius = 0.5 })
    self.entity:Add("RigidBody", { motion = "Dynamic", mass = 2, gravityFactor = 0, linearDamping = 0 })
    physics.AddImpulse(self.entity, vec3(10, 0, 0))
end
return P
)");
        Entity b = w.Make("Ball", glm::vec3(0.0f, 3.0f, 0.0f));
        Attach(b, ball);
        Entity p = w.Make("Pushed", glm::vec3(0.0f, 10.0f, 5.0f));
        Attach(p, pushed);
        w.Frames(120);
        glm::vec3 linear(0.0f), angular(0.0f);
        w.physics.GetVelocity(p, linear, angular);
        // Dropped 2.5 m: it meets the ground at about sqrt(2 g h) = 7.0 m/s (a little more with
        // speculative contacts, which begin just before it touches).
        const std::string name = b.GetName();
        const size_t bar = name.find('|');
        const std::string report = name.substr(0, bar);
        const float speed = bar == std::string::npos ? 0.0f : std::stof(name.substr(bar + 1));
        const bool passed = report == "contacts=1 other=Ground ground=0.000 up=-1" && speed > 6.9f && speed < 7.2f &&
                            std::abs(linear.x - 5.0f) < 1e-3f;
        return Finish(log, "physics", report + Format(", hit at %.3f m/s, pushed at %.3f m/s", speed, linear.x), passed);
    }

    // Entities made, parented, destroyed and found by scripts; scene.Each in id order.
    inline Result Spawning(const Log& log)
    {
        using namespace detail;
        World w;
        const AssetHandle<ScriptAsset> script = w.Source("Spawner.luau", R"(
local S = {}
function S:Start()
    self.made = {}
    for i = 1, 3 do
        local e = scene.Create("Made" .. i)
        e.position = vec3(i, 0, 0)
        e.parent = self.entity
        table.insert(self.made, e)
    end
    self.made[2]:Destroy()
end
function S:Update()
    local names = {}
    scene.Each("Scripts", function(e) table.insert(names, e.name) end)
    local found = self.entity:FindChild("Made3")
    self.entity.name = string.format("children=%d found=%s at=%g valid2=%s same=%s scripted=%d",
        #self.entity:GetChildren(), tostring(found ~= nil), found and found.worldPosition.x or -1,
        tostring(self.made[2]:IsValid()), tostring(found == self.made[3]), #names)
end
return S
)");
        Entity s = w.Make("Spawner", glm::vec3(10.0f, 0.0f, 0.0f));
        Attach(s, script);
        w.Frames(2);
        const std::string name = s.GetName();
        return Finish(log, "spawning", name, name == "children=2 found=true at=13 valid2=false same=true scripted=1");
    }

    // One script calling another's functions (entity:GetScript), and OnDestroy when the game ends.
    inline Result Talking(const Log& log)
    {
        using namespace detail;
        World w;
        const AssetHandle<ScriptAsset> health = w.Source("Health.luau", R"(
local Health = {}
Health.properties = { hp = 100 }
function Health:Damage(n) self.hp -= n end
function Health:OnDestroy() scene.Create("ended with " .. self.hp) end
return Health
)");
        const AssetHandle<ScriptAsset> hurter = w.Source("Hurter.luau", R"(
local Hurter = {}
function Hurter:Update() self.entity:GetScript("Health"):Damage(10) end
function Hurter:LateUpdate() self.entity.name = "hp=" .. self.entity:GetScript("Health").hp end
return Hurter
)");
        Entity e = w.Make("Target");
        Attach(e, health);
        Attach(e, hurter);
        w.Frames(5);
        const std::string name = e.GetName();
        w.scripts.Reset();
        const bool ended = w.scene.FindByName("ended with 50").IsValid();
        return Finish(log, "talking", name + (ended ? ", OnDestroy ran" : ", no OnDestroy"), name == "hp=50" && ended);
    }

    // New source while running: instances keep their state and run the new code.
    inline Result Reload(const Log& log)
    {
        using namespace detail;
        World w;
        const AssetHandle<ScriptAsset> counter = w.Source("Counter.luau", R"(
local C = {}
function C:Start() self.count = 0 end
function C:Update() self.count += 1 end
function C:LateUpdate() self.entity.name = "count=" .. self.count end
return C
)");
        Entity e = w.Make("Counter");
        Attach(e, counter);
        w.Frames(10);
        counter.Get()->SetSource(R"(
local C = {}
function C:Start() self.count = 0 end
function C:Update() self.count += 10 end
function C:LateUpdate() self.entity.name = "count=" .. self.count end
return C
)");
        w.Frames(10);
        const std::string name = e.GetName();
        return Finish(log, "reload", name, name == "count=110");
    }

    // A script that errors stops (with its error); an endless loop is stopped by the time limit;
    // the others go on.
    inline Result Errors(const Log& log)
    {
        using namespace detail;
        World w;
        w.scripts.SetTimeLimit(0.2);
        const AssetHandle<ScriptAsset> boom = w.Source("Boom.luau", R"(
local B = {}
function B:Start() self.n = 0 end
function B:Update() self.n += 1; if self.n == 3 then error("boom") end; self.entity.name = "n=" .. self.n end
return B
)");
        const AssetHandle<ScriptAsset> loop = w.Source("Loop.luau", R"(
local L = {}
function L:Update() while true do end end
return L
)");
        const AssetHandle<ScriptAsset> fine = w.Source("Fine.luau", R"(
local F = {}
function F:Start() self.n = 0 end
function F:Update() self.n += 1; self.entity.name = "n=" .. self.n end
return F
)");
        const AssetHandle<ScriptAsset> broken = w.Source("Broken.luau", "local x = = 1");
        Entity a = w.Make("A");
        Entity b = w.Make("B");
        Entity c = w.Make("C");
        Entity d = w.Make("D");
        Attach(a, boom);
        Attach(b, loop);
        Attach(c, fine);
        Attach(d, broken);
        w.Frames(10);
        const std::string ea = w.scripts.GetError(a, 0), eb = w.scripts.GetError(b, 0), ed = w.scripts.GetError(d, 0);
        const bool passed = std::string(a.GetName()) == "n=2" && ea.find("boom") != std::string::npos && eb.find("too long") != std::string::npos &&
                            std::string(c.GetName()) == "n=10" && !ed.empty() && !broken.Get()->error.empty();
        return Finish(log, "errors", std::string(a.GetName()) + ", " + c.GetName() + ", " + std::to_string(w.scripts.InstanceCount()) + " instances" +
                                         (passed ? "" : " | " + ea + " | " + eb + " | " + ed), passed);
    }

    // A script drives the primitive car (Vehicle.throttle, a field files don't keep) exactly as the
    // physics test does: the same speed after 5 s (MarianaPhysicsTest's "car").
    inline Result Driving(const Log& log)
    {
        using namespace detail;
        World w;
        Entity car = PrimitiveCar(w);
        Attach(car, w.Source("Driver.luau", R"(
local Driver = {}
function Driver:Start() self.frames = 0 end
function Driver:Update()
    self.frames += 1
    self.entity:Get("Vehicle").throttle = if self.frames > 60 then 1 else 0
end
function Driver:LateUpdate()
    local info = physics.GetVehicleInfo(self.entity)
    self.entity.name = string.format("%.1f km/h gear %d", info.speed * 3.6, info.gear)
end
return Driver
)"));
        w.Frames(360);
        VehicleInfo info;
        w.physics.GetVehicleInfo(car, info);
        const float kmh = info.speed * 3.6f;
        return Finish(log, "driving", std::string(car.GetName()) + Format(" (%.3f)", kmh), kmh > 65.0f && kmh < 85.0f && info.gear >= 2);
    }

    // The Sandbox's control scripts: VehicleDriver with W held drives the primitive car as
    // the physics test does (the same 73.8 km/h); then S brakes it to a stop and backs it up, and
    // D steers right; ChaseCamera follows it (further behind the faster it goes: it catches up at
    // `stiffness` per second).
    inline Result StandardVehicle(const Log& log)
    {
        using namespace detail;
        World w;
        Entity car = PrimitiveCar(w);
        if (!ScriptWorld::AddScript(car, w.assets, "/Scripts/VehicleDriver.luau"))
            return Finish(log, "vehicle driver", "no /Scripts/VehicleDriver.luau", false);
        Entity camera = w.Make("Camera", glm::vec3(0.0f, 3.0f, -160.0f));
        camera.Add<CameraComponent>(CameraComponent{});
        ScriptWorld::AddScript(camera, w.assets, "/Scripts/ChaseCamera.luau");
        w.Frames(60);
        w.input.SimulateKey(Key::W, true);
        w.Frames(300);
        VehicleInfo info;
        w.physics.GetVehicleInfo(car, info);
        const float kmh = info.speed * 3.6f;
        const glm::vec3 carAt = Position(car), cameraAt = Position(camera);
        const float behind = carAt.z - cameraAt.z;
        w.input.SimulateKey(Key::W, false);
        w.input.SimulateKey(Key::S, true);
        float braking = 1.0f;
        for (int i = 0; i < 600 && info.speed > -1.0f; ++i)
        {
            w.Frames(1);
            w.physics.GetVehicleInfo(car, info);
            if (info.speed > 0.6f)
                braking = std::min(braking, car.TryGet<Vehicle>()->brake);
        }
        const float reversing = info.speed;
        w.input.SimulateKey(Key::S, false);
        w.input.SimulateKey(Key::D, true);
        w.Frames(30);
        const float steering = car.TryGet<Vehicle>()->steering;
        const bool passed = kmh > 65.0f && kmh < 85.0f && behind > 6.0f && behind < 14.0f && braking == 1.0f && reversing < -0.9f && steering > 0.9f;
        return Finish(log, "vehicle driver",
                      Format("%.3f km/h (W), camera %.2f m behind, braked then backing at %.2f m/s (S), steering %.2f (D)", kmh, behind, reversing, steering),
                      passed);
    }

    // CharacterDriver walks a character where the scene's camera looks (W), runs, jumps (Space); the
    // ChaseCamera turns to follow it.
    inline Result StandardCharacter(const Log& log)
    {
        using namespace detail;
        World w;
        Entity ground = w.Make("Ground", glm::vec3(0.0f, -0.5f, 0.0f));
        BoxCollider groundBox;
        groundBox.size = glm::vec3(100.0f, 1.0f, 100.0f);
        ground.Add<BoxCollider>(groundBox);
        Entity character = w.Make("Character");
        character.Add<CharacterController>(CharacterController{});
        if (!ScriptWorld::AddScript(character, w.assets, "/Scripts/CharacterDriver.luau"))
            return Finish(log, "character driver", "no /Scripts/CharacterDriver.luau", false);
        // The camera looks along +X, so W walks that way.
        Entity camera = w.Make("Camera", glm::vec3(-5.0f, 2.0f, 0.0f));
        const glm::quat alongX = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        camera.SetRotationQuat(alongX.x, alongX.y, alongX.z, alongX.w);
        camera.Add<CameraComponent>(CameraComponent{});
        w.Frames(30);
        w.input.SimulateKey(Key::W, true);
        w.Frames(60);
        const glm::vec3 walked = Position(character);
        w.input.SimulateKey(Key::LEFT_SHIFT, true);
        w.Frames(60);
        const float ran = Position(character).x - walked.x;
        w.input.SimulateKey(Key::W, false);
        w.input.SimulateKey(Key::LEFT_SHIFT, false);
        w.Frames(30);
        w.input.SimulateKey(Key::SPACE, true);
        float top = 0.0f;
        for (int i = 0; i < 90; ++i)
        {
            w.Frames(1);
            if (i == 0)
                w.input.SimulateKey(Key::SPACE, false);
            top = std::max(top, Position(character).y);
        }
        // Now a chase camera behind it.
        ScriptWorld::AddScript(camera, w.assets, "/Scripts/ChaseCamera.luau");
        w.Frames(60);
        const glm::vec3 c = Position(camera), p = Position(character);
        const bool passed = walked.x > 3.0f && std::abs(walked.z) < 0.05f && ran > 6.5f && top > 1.0f && c.x < p.x - 5.0f && c.y > p.y + 2.0f;
        return Finish(log, "character driver",
                      Format("walked to x %.2f (z %.3f), ran %.2f m in 1 s, jumped to %.2f m;", walked.x, walked.z, ran, top) +
                          Format(" camera %.2f m behind and %.2f up", p.x - c.x, c.y - p.y),
                      passed);
    }

    // What a script declares, read without running the game.
    inline Result Describe(const Log& log)
    {
        using namespace detail;
        World w;
        const ScriptInfo& info = w.scripts.Describe(w.Source("Mover.luau", kMover));
        std::string names;
        for (const ScriptValue& v : info.properties)
            names += (names.empty() ? "" : " ") + v.name;
        const bool passed = info.error.empty() && names == "active label offset speed" && info.properties[0].type == ScriptValueType::Bool &&
                            info.properties[1].text == "mover" && info.properties[2].type == ScriptValueType::Vector && info.properties[3].number == 1.0f;
        return Finish(log, "describe", names + (info.error.empty() ? "" : " | " + info.error), passed);
    }

    // A script on a model hears its Animator's events: the name and the clip, each time the clip
    // passes them (after the frame's physics, before LateUpdate).
    inline Result AnimationEvents(const Log& log)
    {
        using namespace detail;
        World w;
        Entity model = w.Make("Dancer");
        const TestRig::Rig rig = TestRig::Make(w.scene, w.assets, model, TestRig::Walker(), "test://dancer");
        Animator animator;
        animator.skeleton = rig.skeleton;
        NamedClip clip = TestRig::WalkClip(w.assets, "Walk", 1.0f);
        clip.events = { { 0.5f, "Clap" } };
        animator.clips = { clip };
        model.Add<Animator>(animator);
        model.Get<Animator>()->Play(0);
        Attach(model, w.Source("Listener.luau", R"(
local L = {}
function L:Start()
    self.heard = {}
end
function L:OnAnimationEvent(name, clip)
    table.insert(self.heard, name .. "@" .. clip .. "@" .. string.format("%.1f", time.time))
    self.entity.name = table.concat(self.heard, " ")
end
return L
)"));
        w.Frames(140); // 2.33 s: claps at 0.5 and 1.5
        const std::string heard = model.GetName();
        return Finish(log, "animation events", heard, heard == "Clap@Walk@0.5 Clap@Walk@1.5");
    }

    // A script aims a model's head with its graph's Look At: animator:SetVector each frame.
    inline Result LookAtFromLua(const Log& log)
    {
        using namespace detail;
        World w;
        Entity model = w.Make("Watcher");
        const TestRig::Rig rig = TestRig::Make(w.scene, w.assets, model,
            { { "Spine", -1, glm::vec3(0.0f, 1.0f, 0.0f) }, { "Neck", 0, glm::vec3(0.0f, 0.3f, 0.0f) }, { "Head", 1, glm::vec3(0.0f, 0.2f, 0.0f) } }, "test://watcher");
        AnimationGraph::Builder b;
        b.Vector("look");
        b.Output(b.LookAt(b.Clip("Hold"), "Head", "look", 2, 90.0f));
        Animator animator;
        animator.skeleton = rig.skeleton;
        animator.clips = { TestRig::Clip(w.assets, "Hold", { TestRig::Track("Spine", { 0.0f, 1.0f }, { glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) }) }) };
        animator.graph = w.assets.RegisterAsset<AnimationGraph>(b.Build(), AssetType::AnimationGraph, "Look", "test://graph/look");
        model.Add<Animator>(animator);
        w.Make("Ball", glm::vec3(3.0f, 1.5f, 0.0f)); // to its right, at head height
        Attach(model, w.Source("Watch.luau", R"(--!strict
local W = {} :: Script
function W.Update(self: Script, dt: number)
    local ball = scene.Find("Ball")
    local animator = self.entity:Get("Animator")
    if ball and animator then
        animator:SetVector("look", ball.worldPosition)
    end
end
return W
)"));
        w.Frames(3);
        const glm::vec3 facing = glm::normalize(glm::mat3(WorldMatrix(w.scene, rig.bones[2])) * glm::vec3(0.0f, 0.0f, 1.0f));
        const float off = glm::degrees(std::acos(glm::clamp(glm::dot(facing, glm::vec3(1.0f, 0.0f, 0.0f)), -1.0f, 1.0f)));
        return Finish(log, "look at from Lua", Format("the head faces the ball %.3f deg off", off), off < 0.5f);
    }

    // The Sandbox's WatchCar.luau: a dancer near the driven car aims its graph's Look At at it and
    // eases the look's weight in.
    inline Result WatchCar(const Log& log)
    {
        using namespace detail;
        World w;
        Entity car = w.Make("Car", glm::vec3(3.0f, 0.0f, 0.0f));
        ScriptWorld::AddScript(car, w.assets, "/Scripts/VehicleDriver.luau");
        Entity model = w.Make("Dancer", glm::vec3(0.0f, 0.0f, 5.0f));
        const TestRig::Rig rig = TestRig::Make(w.scene, w.assets, model,
            { { "Spine", -1, glm::vec3(0.0f, 1.0f, 0.0f) }, { "Neck", 0, glm::vec3(0.0f, 0.3f, 0.0f) }, { "Head", 1, glm::vec3(0.0f, 0.2f, 0.0f) } }, "test://dancer");
        AnimationGraph::Builder b;
        b.Vector("lookTarget");
        b.Float("lookWeight");
        b.Output(b.LookAt(b.Clip("Hold"), "Head", "lookTarget", 3, 70.0f, b.Parameter("lookWeight")));
        Animator animator;
        animator.skeleton = rig.skeleton;
        animator.clips = { TestRig::Clip(w.assets, "Hold", { TestRig::Track("Spine", { 0.0f, 1.0f }, { glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) }) }) };
        animator.graph = w.assets.RegisterAsset<AnimationGraph>(b.Build(), AssetType::AnimationGraph, "Dancer", "test://graph/dancer");
        model.Add<Animator>(animator);
        if (!ScriptWorld::AddScript(model, w.assets, "/Scripts/WatchCar.luau"))
            return Finish(log, "watch car", "no /Scripts/WatchCar.luau", false);
        w.Frames(120);
        const Animator& a = *model.TryGet<Animator>();
        const glm::vec3 target = a.GetVector("lookTarget");
        const float weight = a.GetFloat("lookWeight");
        const std::string error = w.scripts.GetError(model, 0);
        return Finish(log, "watch car", Format("looks at (%.2f, %.2f, %.2f) with weight %.3f", target.x, target.y, target.z, weight) + (error.empty() ? "" : " | " + error),
                      std::abs(target.x - 3.0f) < 1e-3f && std::abs(target.y - 1.0f) < 1e-3f && weight > 0.99f && error.empty());
    }

    // The Sandbox's Snake.luau (the tutorial's game): it builds its board, eats an apple put in
    // front of it and grows, runs into the wall, flies apart as physics bodies and starts again.
    inline Result SnakeTutorial(const Log& log)
    {
        using namespace detail;
        World w;
        Entity game = w.Make("Game");
        if (!ScriptWorld::AddScript(game, w.assets, "/Scripts/Snake.luau"))
            return Finish(log, "snake", "no /Scripts/Snake.luau", false);
        // Reads the snake's own state (its `self`) and reports it in its entity's name.
        Entity watcher = w.Make("Watcher");
        Attach(watcher, w.Source("Watcher", R"(
            local Watcher = {}
            function Watcher.Start(self) self.frame = 0 end
            function Watcher.Update(self, dt)
                self.frame += 1
                local snake = scene.Find("Game"):GetScript("Snake")
                if not snake or not snake.cells then return end
                if self.frame == 2 then
                    snake.appleCell = snake.cells[1] + vec3(1, 0, 0)
                    self.start = #snake.segments
                end
                if snake.score == 1 and not self.grown then self.grown = #snake.segments end
                if snake.over and not self.overAt then
                    self.overAt = self.frame
                    self.flying = snake.segments[1]:Has("RigidBody")
                    self.head = snake.cells[1]
                end
                if self.overAt and not snake.over then self.restarted = #snake.segments end
                local walls = 0
                scene.Each("BoxCollider", function(e) if e.name == "Wall" then walls += 1 end end)
                self.entity.name = `{self.start}>{self.grown} over@{self.overAt} at {self.head} flying {self.flying} restart {self.restarted} walls {walls}`
            end
            return Watcher)"));
        w.Frames(420);
        const std::string report = watcher.GetName();
        const std::string error = w.scripts.GetError(game, 0) + w.scripts.GetError(watcher, 0);
        return Finish(log, "snake", report + (error.empty() ? "" : " | " + error),
                      error.empty() && report.rfind("3>4 over@", 0) == 0 && report.find("flying true") != std::string::npos &&
                          report.find("restart 3") != std::string::npos && report.find("walls 4") != std::string::npos);
    }

    // The documentation's Snake tutorial, stage by stage (docs/snippets/snake/part1..4.luau): each
    // runs for 8 s with the snake steered up and left without an error, and builds its board.
    inline Result SnakeTutorialStages(const Log& log)
    {
        using namespace detail;
        std::string report;
        bool passed = true;
        for (int part = 1; part <= 4; ++part)
        {
            const std::filesystem::path file = std::filesystem::path(MARIANA_SANDBOX_DIR) / ".." / ".." / "docs" / "snippets" / "snake" /
                                               ("part" + std::to_string(part) + ".luau");
            std::ifstream stream(file, std::ios::binary);
            std::ostringstream text;
            text << stream.rdbuf();
            if (text.str().empty())
                return Finish(log, "snake tutorial stages", "no " + file.string(), false);
            World w;
            Entity game = w.Make("Game");
            const std::string name = "Part" + std::to_string(part);
            Attach(game, w.Source(name.c_str(), text.str().c_str()));
            w.Frames(30);
            const Key turns[] = { Key::UP, Key::LEFT };
            for (Key key : turns)
            {
                w.input.SimulateKey(key, true);
                w.Frames(1);
                w.input.SimulateKey(key, false);
                w.Frames(20);
            }
            w.Frames(420);
            const std::string error = w.scripts.GetError(game, 0);
            const bool board = w.scene.FindByName("Wall").IsValid() && w.scene.FindByName("Segment").IsValid();
            report += (report.empty() ? "" : ", ") + name + (error.empty() ? (board ? " ok" : " no board") : " | " + error);
            passed = passed && error.empty() && board;
        }
        return Finish(log, "snake tutorial stages", report, passed);
    }

    // The documentation's cookbook and API examples (docs/snippets/cookbook, api): each script runs for 3 s on an entity in
    // an empty scene, with keys and the mouse pressed, without an error (a recipe must cope with what
    // it looks for not being there).
    inline Result CookbookRuns(const Log& log)
    {
        using namespace detail;
        namespace fs = std::filesystem;
        std::string report;
        int ran = 0;
        const fs::path docs = fs::path(MARIANA_SANDBOX_DIR) / ".." / ".." / "docs" / "snippets";
        std::vector<fs::path> files;
        std::error_code ec;
        for (const char* folder : { "cookbook", "api" })
            for (const auto& entry : fs::directory_iterator(docs / folder, ec))
                files.push_back(entry.path());
        for (const fs::path& path : files)
        {
            if (path.extension() != ".luau")
                continue;
            std::ifstream stream(path, std::ios::binary);
            std::ostringstream text;
            text << stream.rdbuf();
            World w;
            Entity tester = w.Make("Tester", glm::vec3(0.0f, 1.0f, 0.0f));
            const std::string name = path.stem().string();
            Attach(tester, w.Source(name.c_str(), text.str().c_str()));
            const Key keys[] = { Key::W, Key::SPACE, Key::X, Key::F, Key::TAB, Key::E };
            for (Key key : keys)
            {
                w.input.SimulateKey(key, true);
                w.Frames(20);
                w.input.SimulateKey(key, false);
                w.Frames(10);
            }
            ++ran;
            if (const std::string error = w.scripts.GetError(tester, 0); !error.empty())
                report += " | " + name + ": " + error;
        }
        return Finish(log, "cookbook runs", std::to_string(ran) + " scripts" + (report.empty() ? ", no errors" : report), ran >= 15 && report.empty());
    }

    inline std::vector<Result> RunAll(const Log& log)
    {
        return { Lifecycle(log), Components(log), Physics(log), Spawning(log), Talking(log), Reload(log), Errors(log), Driving(log),
                 StandardVehicle(log), StandardCharacter(log), Describe(log), AnimationEvents(log), LookAtFromLua(log), WatchCar(log), SnakeTutorial(log), SnakeTutorialStages(log), CookbookRuns(log) };
    }
}
