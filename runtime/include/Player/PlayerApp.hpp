#pragma once
#include <Mariana/Application.hpp>
#include <Mariana/Rendering/Renderer.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>
#include <Mariana/Rendering/SceneView.hpp>
#include <Mariana/Rendering/Effect.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Physics/Controllers.hpp>

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// What the player runs, from a project's Player.json (until scenes can be saved):
//   {
//     "window": { "title": "Mariana Player", "width": 1280, "height": 720, "icon": "/logo.png" },
//     "pipeline": "/Pipelines/Default.pipeline",
//     "camera": { "position": [0, 2, -5], "target": [0, 1, 0], "orbitSpeed": 0, "follow": false },
//     "spawn": [ { "prefab": "/Models/Sponza/Sponza.gltf", "position": [0, 0, 0], "rotation": [0, 0, 0],
//                  "scale": 1, "faceCamera": false, "physics": { "collider": "mesh" } },
//                { "primitive": "sphere", "color": [1, 0.3, 0.2], "position": [0, 5, 0],
//                  "physics": { "body": "dynamic", "mass": 2, "bounciness": 0.5 } }, ... ]
//   }
// Everything is optional; paths are resource paths (FileSystem). A spawn is a `prefab`, a
// `primitive` (cube, sphere, plane, cylinder, capsule; tinted by `color`) or a `preset` (car,
// motorcycle, tank: a vehicle the keyboard drives, W A S D and Space; playground: ground with ramps).
// With `follow` (and optional `distance`, `height`) the camera chases the driven vehicle. `rotation` is Euler angles
// in degrees; `scale` a number or [x, y, z]; `faceCamera` instead turns it about +Y so its front (+Z,
// as glTF assets face) points at the camera's starting position. `physics` makes it a body: `body`
// static (default), kinematic or dynamic; `collider` box, sphere, capsule or cylinder (one on the
// root, fitted to the meshes), mesh or convex (a mesh collider on every mesh), or none; it defaults to
// a primitive's own shape, else mesh for static bodies and convex for moving ones. `mass` (kg, 0 =
// from density), `friction`, `bounciness` and `velocity` (m/s) are optional too.
struct PlayerConfig
{
    struct Physics
    {
        bool enabled = false;
        std::string body = "static";
        std::string collider; // empty: the default for the spawn
        float mass = 1.0f;
        float friction = 0.5f;
        float bounciness = 0.0f;
        glm::vec3 velocity{0.0f};
    };

    struct Spawn
    {
        std::string prefab;
        std::string primitive;
        std::string preset;
        glm::vec3 color{1.0f};
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f}; // Euler degrees
        glm::vec3 scale{1.0f};
        bool faceCamera = false;
        Physics physics;
    };

    WindowSettings window{ "Mariana Player", 1280, 720, "" };
    std::string pipeline = "/Pipelines/Default.pipeline";
    glm::vec3 cameraPosition{0.0f, 2.0f, -5.0f};
    glm::vec3 cameraTarget{0.0f, 1.0f, 0.0f};
    float orbitSpeed = 0.0f; // degrees per second around the target
    bool follow = false;
    FollowCamera chase;
    std::vector<Spawn> spawns;

    static bool Load(const std::string& path, PlayerConfig& out, std::string& error);
};

// Looks at a target from a position, optionally circling it.
class PlayerCamera final : public ICamera
{
public:
    PlayerCamera(IInput& input, const glm::vec3& position, const glm::vec3& target, float orbitSpeed);
    void OnUpdate(float deltaTime) override;
    void Place(const glm::vec3& position, const glm::vec3& lookAt) { _pos = position; target = lookAt; }

private:
    glm::vec3 target;
    float orbitSpeed;
};

class PhysicsWorld;

// Runs a project without the editor: the scene, its animation and physics and the render pipeline,
// no UI.
class PlayerApp : public Application
{
public:
    explicit PlayerApp(const PlayerConfig& config);
    ~PlayerApp() override;

protected:
    void OnStart() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;

private:
    PlayerConfig config;
    // Declared first: the scene view and the effects remove their passes from its graph.
    Renderer renderer;
    Scene scene;
    SceneRenderer sceneRenderer;
    SceneView sceneView;
    EffectStack effects;
    // The scene's simulation (with the project's /Physics.json, if it has one), always running.
    std::unique_ptr<PhysicsWorld> physics;
    // Systems query `scene`, so they are declared after it to be destroyed first.
    System animationSystem;

    void OnWindowResized(int width, int height);
    Entity SpawnPrimitive(const PlayerConfig::Spawn& spawn);
    ChaseCamera chase;
    void AddPhysics(Entity root, const PlayerConfig::Spawn& spawn);
};
