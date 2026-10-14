#include <Player/PlayerApp.hpp>

#include <Mariana/Animation/AnimationPlayer.hpp>
#include <Mariana/Foundation/FileReader.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Rendering/RenderPipeline.hpp>
#include <Mariana/Scene/Prefab.hpp>

#include <json.hpp>

#include <cmath>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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
        }
        if (const auto spawns = file.find("spawn"); spawns != file.end() && spawns->is_array())
        {
            for (const Json& entry : *spawns)
            {
                Spawn spawn;
                spawn.prefab = entry.value("prefab", std::string());
                spawn.position = ReadVec3(entry, "position", spawn.position);
                spawn.rotation = ReadVec3(entry, "rotation", spawn.rotation);
                spawn.scale = entry.value("scale", spawn.scale);
                spawn.faceCamera = entry.value("faceCamera", spawn.faceCamera);
                if (!spawn.prefab.empty())
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
      effects(renderer.graph, sceneRenderer.CameraBuffer())
{
    AnimationPlayer::RegisterComponents();
    SceneRenderer::RegisterComponents();
    scene.UpdateComponentRegistry();

    cam = std::make_unique<PlayerCamera>(input, config.cameraPosition, config.cameraTarget, config.orbitSpeed);
    cam->SetViewportSize(m_Window.GetWidth(), m_Window.GetHeight());

    renderer.graph.SetBackbufferFormat(m_Window.GetWindowFormat());
    renderer.graph.SetOutputSize(m_Window.GetWidth(), m_Window.GetHeight());
    sceneRenderer.writeEntityIds = false; // nothing picks
    sceneRenderer.Init(renderer.graph);
    GLTF::GLTFLoader::RegisterLoader(assets);
    MaterialFile::RegisterLoader(assets);

    RenderPipelineDesc pipeline;
    std::string error;
    if (RenderPipelineDesc::Load(config.pipeline, pipeline, error))
        pipeline.Apply(effects, sceneRenderer, cam.get());
    else
        Logger::Error("Could not load the render pipeline: " + error);

    animationSystem = scene.CreateSystem<AnimationPlayer>([](Entity e, AnimationPlayer& player, float dt) {
        player.Update(e, dt);
    });

    m_Window.RegisterResizeCallback([this](int w, int h) { OnWindowResized(w, h); });
}

void PlayerApp::OnStart()
{
    for (const PlayerConfig::Spawn& spawn : config.spawns)
    {
        const std::filesystem::path file = FileSystem::Resolve(spawn.prefab);
        std::shared_ptr<Prefab> prefab = file.empty() ? nullptr : assets.Load<Prefab>(file).Get();
        if (!prefab)
        {
            Logger::Error("Could not spawn " + spawn.prefab);
            continue;
        }
        glm::quat rotation = glm::quat(glm::radians(spawn.rotation));
        if (spawn.faceCamera)
        {
            const glm::vec3 toCamera = config.cameraPosition - spawn.position;
            rotation = glm::angleAxis(std::atan2(toCamera.x, toCamera.z), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        Entity root = scene.Instantiate(*prefab, file.stem().string().c_str());
        root.SetPosition(spawn.position.x, spawn.position.y, spawn.position.z);
        root.SetRotationQuat(rotation.x, rotation.y, rotation.z, rotation.w);
        root.SetScale(spawn.scale, spawn.scale, spawn.scale);
    }
    scene.Update(0.0f);
}

void PlayerApp::OnUpdate(float deltaTime)
{
    animationSystem.Run(deltaTime);
}

void PlayerApp::OnRender()
{
    scene.Update(0.0f);
    sceneRenderer.Prepare(*cam);
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
