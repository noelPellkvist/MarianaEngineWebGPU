#pragma once
#include <Mariana/Application.hpp>
#include <Mariana/Rendering/Renderer.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Rendering/Effect.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <string>
#include <vector>
#include <glm/glm.hpp>

// What the player runs, from a project's Player.json (until scenes can be saved):
//   {
//     "window": { "title": "Mariana Player", "width": 1280, "height": 720, "icon": "/logo.png" },
//     "pipeline": "/Pipelines/Default.pipeline",
//     "camera": { "position": [0, 2, -5], "target": [0, 1, 0], "orbitSpeed": 0 },
//     "spawn": [ { "prefab": "/Models/Sponza/Sponza.gltf", "position": [0, 0, 0], "scale": 1 }, ... ]
//   }
// Everything is optional; paths are resource paths (FileSystem).
struct PlayerConfig
{
    struct Spawn
    {
        std::string prefab;
        glm::vec3 position{0.0f};
        float scale = 1.0f;
    };

    WindowSettings window{ "Mariana Player", 1280, 720, "" };
    std::string pipeline = "/Pipelines/Default.pipeline";
    glm::vec3 cameraPosition{0.0f, 2.0f, -5.0f};
    glm::vec3 cameraTarget{0.0f, 1.0f, 0.0f};
    float orbitSpeed = 0.0f; // degrees per second around the target
    std::vector<Spawn> spawns;

    static bool Load(const std::string& path, PlayerConfig& out, std::string& error);
};

// Looks at a target from a position, optionally circling it.
class PlayerCamera final : public ICamera
{
public:
    PlayerCamera(IInput& input, const glm::vec3& position, const glm::vec3& target, float orbitSpeed);
    void OnUpdate(float deltaTime) override;

private:
    glm::vec3 target;
    float orbitSpeed;
};

// Runs a project without the editor: the scene, its animation and the render pipeline, no UI.
class PlayerApp : public Application
{
public:
    explicit PlayerApp(const PlayerConfig& config);

protected:
    void OnStart() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;

private:
    PlayerConfig config;
    // Declared first: the scene renderer and the effects remove their passes from its graph.
    Renderer renderer;
    Scene scene;
    SceneRenderer sceneRenderer;
    EffectStack effects;
    // Systems query `scene`, so they are declared after it to be destroyed first.
    System animationSystem;

    void OnWindowResized(int width, int height);
};
