#include <Player/PlayerApp.hpp>

#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Foundation/FileReader.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Importers/AnimationGraphFile.hpp>
#include <Mariana/Physics/ColliderGeometry.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Physics/VehiclePresets.hpp>
#include <Mariana/Physics/CharacterPresets.hpp>
#include <Mariana/Physics/JointPresets.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Rendering/RenderPipeline.hpp>
#include <Mariana/Scene/Prefab.hpp>

#include <json.hpp>

#include <cmath>
#include <filesystem>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{
    using Json = nlohmann::json;

    glm::vec3 ReadVec3(const Json& object, const char* key, const glm::vec3& fallback)
    {
        const auto it = object.find(key);
        if (it == object.end() || !it->is_array() || it->size() != 3)
            return fallback;
        return glm::vec3((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>());
    }
}

bool PlayerConfig::Load(const std::string& path, PlayerConfig& out, std::string& error)
{
    try
    {
        const Json file = Json::parse(FileReader::LoadRawString(path));
        if (const auto window = file.find("window"); window != file.end() && window->is_object())
        {
            out.window.title = window->value("title", out.window.title);
            out.window.width = window->value("width", out.window.width);
            out.window.height = window->value("height", out.window.height);
            out.window.iconPath = window->value("icon", out.window.iconPath);
        }
        out.pipeline = file.value("pipeline", out.pipeline);
        if (const auto camera = file.find("camera"); camera != file.end() && camera->is_object())
        {
            out.cameraPosition = ReadVec3(*camera, "position", out.cameraPosition);
            out.cameraTarget = ReadVec3(*camera, "target", out.cameraTarget);
            out.orbitSpeed = camera->value("orbitSpeed", out.orbitSpeed);
            out.follow = camera->value("follow", out.follow);
            out.chase.distance = camera->value("distance", out.chase.distance);
            out.chase.height = camera->value("height", out.chase.height);
        }
        if (const auto spawns = file.find("spawn"); spawns != file.end() && spawns->is_array())
        {
            for (const Json& entry : *spawns)
            {
                Spawn spawn;
                spawn.prefab = entry.value("prefab", std::string());
                spawn.primitive = entry.value("primitive", std::string());
                spawn.preset = entry.value("preset", std::string());
                spawn.vehicle = entry.value("vehicle", std::string());
                spawn.color = ReadVec3(entry, "color", spawn.color);
                spawn.position = ReadVec3(entry, "position", spawn.position);
                spawn.rotation = ReadVec3(entry, "rotation", spawn.rotation);
                if (const auto scale = entry.find("scale"); scale != entry.end() && scale->is_number())
                    spawn.scale = glm::vec3(scale->get<float>());
                else
                    spawn.scale = ReadVec3(entry, "scale", spawn.scale);
                spawn.faceCamera = entry.value("faceCamera", spawn.faceCamera);
                if (const auto physics = entry.find("physics"); physics != entry.end() && physics->is_object())
                {
                    Physics& p = spawn.physics;
                    p.enabled = true;
                    p.body = physics->value("body", p.body);
                    p.collider = physics->value("collider", p.collider);
                    p.mass = physics->value("mass", p.mass);
                    p.friction = physics->value("friction", p.friction);
                    p.bounciness = physics->value("bounciness", p.bounciness);
                    p.velocity = ReadVec3(*physics, "velocity", p.velocity);
                }
                if (!spawn.prefab.empty() || !spawn.primitive.empty() || !spawn.preset.empty())
                    out.spawns.push_back(spawn);
            }
        }
    }
    catch (const std::exception& e)
    {
        error = path + ": " + e.what();
        return false;
    }
    return true;
}

PlayerCamera::PlayerCamera(IInput& input, const glm::vec3& position, const glm::vec3& target, float orbitSpeed)
    : ICamera(input), target(target), orbitSpeed(orbitSpeed)
{
    _pos = position;
    _view = glm::lookAtLH(_pos, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void PlayerCamera::OnUpdate(float deltaTime)
{
    if (orbitSpeed != 0.0f)
    {
        const float angle = glm::radians(orbitSpeed) * deltaTime;
        const glm::vec3 offset = _pos - target;
        const float c = std::cos(angle), s = std::sin(angle);
        _pos = target + glm::vec3(offset.x * c - offset.z * s, offset.y, offset.x * s + offset.z * c);
    }
    _view = glm::lookAtLH(_pos, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

PlayerApp::PlayerApp(const PlayerConfig& config)
    : Application(config.window),
      config(config),
      sceneRenderer(scene, assets),
      sceneView(sceneRenderer),
      effects(renderer.graph, sceneView.CameraBuffer())
{
    Animator::RegisterComponents();
    SceneRenderer::RegisterComponents();
    PhysicsWorld::RegisterComponents();
    scene.UpdateComponentRegistry();

    cam = std::make_unique<PlayerCamera>(input, config.cameraPosition, config.cameraTarget, config.orbitSpeed);
    cam->SetViewportSize(m_Window.GetWidth(), m_Window.GetHeight());

    renderer.graph.SetBackbufferFormat(m_Window.GetWindowFormat());
    renderer.graph.SetOutputSize(m_Window.GetWidth(), m_Window.GetHeight());
    sceneRenderer.Init();
    sceneView.writeEntityIds = false; // nothing picks
    sceneView.Init(renderer.graph);
    GLTF::GLTFLoader::RegisterLoader(assets);
    MaterialFile::RegisterLoader(assets);
    AnimationGraphFile::RegisterLoader(assets);

    RenderPipelineDesc pipeline;
    std::string error;
    if (RenderPipelineDesc::Load(config.pipeline, pipeline, error))
        pipeline.Apply(effects, sceneRenderer, cam.get());
    else
        Logger::Error("Could not load the render pipeline: " + error);

    animationSystem = Animator::CreateSystem(scene);

    PhysicsSettings physicsSettings;
    if (const std::filesystem::path physicsFile = FileSystem::Resolve("/Physics.json"); !physicsFile.empty())
        if (std::string physicsError; !physicsSettings.Load(physicsFile, physicsError))
            Logger::Warning(physicsError);
    physics = std::make_unique<PhysicsWorld>(scene, physicsSettings);

    m_Window.RegisterResizeCallback([this](int w, int h) { OnWindowResized(w, h); });
}

PlayerApp::~PlayerApp() = default;

Entity PlayerApp::SpawnPrimitive(const PlayerConfig::Spawn& spawn)
{
    static const std::pair<const char*, BuiltinMesh> kShapes[] = {
        { "cube", BuiltinMesh::Cube }, { "sphere", BuiltinMesh::Sphere }, { "plane", BuiltinMesh::Plane },
        { "cylinder", BuiltinMesh::Cylinder }, { "capsule", BuiltinMesh::Capsule },
    };
    for (const auto& [name, shape] : kShapes)
    {
        if (spawn.primitive == name)
            return BuiltinAssets::InstantiatePrimitive(scene, assets, shape, name, spawn.color);
    }
    return {};
}

// The spawn's `physics` block as components: a RigidBody on the root unless it is static, and the
// colliders (fitted to the root's meshes, or a mesh collider per mesh).
void PlayerApp::AddPhysics(Entity root, const PlayerConfig::Spawn& spawn)
{
    const PlayerConfig::Physics& p = spawn.physics;
    const bool moving = p.body == "dynamic" || p.body == "kinematic";
    std::string collider = p.collider;
    if (collider.empty())
        collider = !spawn.primitive.empty() ? (spawn.primitive == "plane" || spawn.primitive == "cube" ? "box" : spawn.primitive) : moving ? "convex" : "mesh";

    auto surface = [&](Collider& c) {
        c.friction = p.friction;
        c.restitution = p.bounciness;
    };
    if (collider == "mesh" || collider == "convex")
    {
        std::function<void(Entity)> visit = [&](Entity e) {
            if (e.TryGet<MeshComponent>())
            {
                MeshCollider c;
                c.convex = collider == "convex";
                surface(c);
                e.Add<MeshCollider>(c);
            }
            e.ForEachChild(visit);
        };
        visit(root);
    }
    else if (collider == "box" || collider == "sphere" || collider == "capsule" || collider == "cylinder")
    {
        if (collider == "box") { BoxCollider c; surface(c); root.Add<BoxCollider>(c); }
        if (collider == "sphere") { SphereCollider c; surface(c); root.Add<SphereCollider>(c); }
        if (collider == "capsule") { CapsuleCollider c; surface(c); root.Add<CapsuleCollider>(c); }
        if (collider == "cylinder") { CylinderCollider c; surface(c); root.Add<CylinderCollider>(c); }
        FitColliders(root);
    }
    else if (collider != "none")
    {
        Logger::Warning("Player.json: unknown collider \"" + collider + "\"");
    }

    if (moving)
    {
        RigidBody body;
        body.motion = p.body == "dynamic" ? MotionType::Dynamic : MotionType::Kinematic;
        body.mass = std::max(p.mass, 0.0f);
        body.startVelocity = p.velocity;
        root.Add<RigidBody>(body);
    }
    else if (p.body != "static")
    {
        Logger::Warning("Player.json: unknown body \"" + p.body + "\"");
    }
}

void PlayerApp::OnStart()
{
    for (const PlayerConfig::Spawn& spawn : config.spawns)
    {
        Entity root;
        if (!spawn.preset.empty())
        {
            const glm::quat turn = glm::quat(glm::radians(spawn.rotation));
            if (spawn.preset == "car" || spawn.preset == "motorcycle" || spawn.preset == "tank")
            {
                const VehicleType type = spawn.preset == "car" ? VehicleType::Wheeled : spawn.preset == "motorcycle" ? VehicleType::Motorcycle : VehicleType::Tracked;
                VehiclePresets::Spawn(scene, assets, type, spawn.position, turn);
            }
            else if (spawn.preset == "playground")
                VehiclePresets::SpawnPlayground(scene, assets, spawn.position);
            else if (spawn.preset == "character")
                CharacterPresets::Spawn(scene, assets, spawn.position, turn);
            else if (spawn.preset == "course")
                CharacterPresets::SpawnCourse(scene, assets, spawn.position);
            else if (spawn.preset == "chain")
                JointPresets::SpawnChain(scene, assets, spawn.position);
            else if (spawn.preset == "door")
                JointPresets::SpawnDoor(scene, assets, spawn.position);
            else if (spawn.preset == "spinner")
                JointPresets::SpawnSpinner(scene, assets, spawn.position);
            else if (spawn.preset == "wrecking ball")
                JointPresets::SpawnWreckingBall(scene, assets, spawn.position);
            else
                Logger::Error("Unknown preset " + spawn.preset);
            continue;
        }
        if (!spawn.primitive.empty())
        {
            root = SpawnPrimitive(spawn);
            if (!root.IsValid())
            {
                Logger::Error("Unknown primitive " + spawn.primitive);
                continue;
            }
        }
        else
        {
            const std::filesystem::path file = FileSystem::Resolve(spawn.prefab);
            std::shared_ptr<Prefab> prefab = file.empty() ? nullptr : assets.Load<Prefab>(file).Get();
            if (!prefab)
            {
                Logger::Error("Could not spawn " + spawn.prefab);
                continue;
            }
            root = scene.Instantiate(*prefab, file.stem().string().c_str());
        }
        glm::quat rotation = glm::quat(glm::radians(spawn.rotation));
        if (spawn.faceCamera)
        {
            const glm::vec3 toCamera = config.cameraPosition - spawn.position;
            rotation = glm::angleAxis(std::atan2(toCamera.x, toCamera.z), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        root.SetPosition(spawn.position.x, spawn.position.y, spawn.position.z);
        root.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
        root.SetScale(spawn.scale.x, spawn.scale.y, spawn.scale.z);
        if (spawn.physics.enabled)
        {
            scene.Update(0.0f); // fitting reads the meshes' world transforms
            AddPhysics(root, spawn);
        }
        if (spawn.vehicle == "car")
        {
            scene.Update(0.0f); // rigging reads the world transforms
            VehiclePresets::RigCar(scene, root);
        }
        else if (!spawn.vehicle.empty())
            Logger::Warning("Player.json: unknown vehicle \"" + spawn.vehicle + "\"");
    }
    scene.Update(0.0f);
    // A player is the game: it plays from the start.
    gameClock.Play();
}

void PlayerApp::OnUpdate(float deltaTime)
{
    animationSystem.Run(gameClock.DeltaTime());
    physics->UpdatePlayerControls(input, gameClock.DeltaTime(), glm::vec3(cam->GetCameraInfo().invView[2]));
    physics->Step(gameClock.DeltaTime());
    UpdateFollowCameras(scene, gameClock.DeltaTime());

    // The camera chases the driven vehicle.
    if (config.follow)
    {
        Entity driven;
        std::function<void(Entity)> find = [&](Entity e) {
            if (!driven.IsValid() && e.TryGet<PlayerControl>())
                driven = e;
            e.ForEachChild(find);
        };
        scene.ForEachRoot([&](Entity root) { find(root); });
        if (const WorldXform* world = driven.IsValid() ? driven.TryGet<WorldXform>() : nullptr)
        {
            chase.Update(glm::make_mat4(world->model), config.chase, deltaTime);
            static_cast<PlayerCamera*>(cam.get())->Place(chase.position, chase.lookAt);
        }
    }
}

void PlayerApp::OnRender()
{
    scene.Update(0.0f);
    sceneRenderer.Update();
    sceneView.Prepare(*cam);
    effects.Update();
    renderer.Render();
}

void PlayerApp::OnWindowResized(int width, int height)
{
    // Minimized: nothing to render into.
    if (width <= 0 || height <= 0)
        return;

    m_Window.GetSurface();
    cam->SetViewportSize(width, height);
    renderer.graph.SetOutputSize(width, height);
}
