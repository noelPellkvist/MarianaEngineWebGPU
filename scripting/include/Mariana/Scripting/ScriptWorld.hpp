#pragma once
#include <Mariana/Scripting/Script.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class AssetManager2;
class EffectStack;
class GameClock;
class ICamera;
class IInput;
class PhysicsWorld;
struct LightingSettings;

// What scripts draw to see what they are doing (gizmos.Line, Sphere, Box, Text), in world space; the
// editor draws them over its Scene and Game views. Each lasts until `until` (game time).
struct ScriptDebugShape
{
    enum class Type : uint8_t { Line, Sphere, Box, Text };
    Type type = Type::Line;
    glm::vec3 a{ 0.0f };                       // a line's start; the center of the others
    glm::vec3 b{ 0.0f };                       // a line's end; a box's size
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f }; // a box's
    float radius = 0.0f;                       // a sphere's
    uint32_t color = 0xffffffffu;              // RGBA8, red in the low byte (as ImGui's)
    std::string text;
    double until = 0.0;
};

// What a script declares, read without running the game (for the inspector).
struct ScriptInfo
{
    std::vector<ScriptValue> properties; // its `properties` with their defaults, by name
    std::string error;                   // why it couldn't be read (empty when fine)
};

// Runs the scripts of a scene (one per scene, owned by the app, like PhysicsWorld): a Luau state
// made when the game starts, a module per script file and an instance (`self`) per script on an
// entity, in entity id order. Scripts reach entities and their components (by name, through the
// fields scene files save), input, the game clock and physics; scripting/API.md is the whole API.
// Errors are logged and stop the script that made them until its file changes.
class ScriptWorld
{
public:
    ScriptWorld(Scene& scene, AssetManager2& assets, GameClock& clock);
    ~ScriptWorld();
    ScriptWorld(const ScriptWorld&) = delete;
    ScriptWorld& operator=(const ScriptWorld&) = delete;

    // The Scripts component and its scene file serializer; before Scene::UpdateComponentRegistry.
    static void RegisterComponents();
    // Puts the script at `resource` (a resource path) on the entity after the ones it has, with
    // `values` for its properties; false when it can't be loaded.
    static bool AddScript(Entity entity, AssetManager2& assets, const std::string& resource, std::vector<ScriptValue> values = {});

    // Physics for scripts (queries, forces, contact callbacks); none by default.
    void SetPhysics(PhysicsWorld* physics);
    // How long one call into a script may run before it is stopped as an endless loop (2 s).
    void SetTimeLimit(double seconds);
    // What the game is seen through, each frame: the game view's rectangle in window pixels
    // (x, y, width, height; mouse positions and screen points are relative to it) and its camera.
    void SetView(const glm::vec4& viewport, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& position);
    // What scripts may change of the picture (the lighting, the post effects and the exposure).
    void SetLighting(LightingSettings* lighting);
    void SetEffects(EffectStack* effects, ICamera* exposure);
    // Where `save` keeps its data: a JSON file natively, or a key in the page's localStorage on the
    // web (`location` is then the key). Read the first time a script uses it.
    void SetSaveLocation(const std::string& location);
    // Writes save data scripts changed (also done after each LateUpdate and on Reset).
    void FlushSave();

    // What scripts asked of the app since it last asked: a scene to load (`reload`: the current one
    // again, `file` empty); false when nothing was asked.
    bool TakeSceneRequest(std::string& file, bool& reload);
    // The cursor as scripts want it (input.SetCursorLocked / SetCursorHidden); the app applies it
    // while the game has the keys. ReleaseCursor forgets it (the editor's Stop, Escape).
    bool CursorLocked() const;
    bool CursorHidden() const;
    void ReleaseCursor();
    // The shapes scripts drew that are still to be shown.
    const std::vector<ScriptDebugShape>& DebugShapes() const;

    // Each frame the game advances, before animation and physics: starts the scripts that are new
    // (Start), then updates them (Update(dt)). Without `input` keys and buttons read as up (the
    // editor's text fields have them).
    void Update(float dt, const IInput* input);
    // FixedUpdate(dt) runs before each fixed physics step (SetPhysics hooks it in).
    // After physics: the contacts of its last step (OnContactBegin/End, OnTriggerEnter/Exit) and
    // LateUpdate(dt).
    void LateUpdate(float dt);
    // Ends every script (OnDestroy) and drops the Luau state: on Play and Stop, so every run starts
    // the same way.
    void Reset();

    // Script files changed on disk are compiled again; running instances take the new code and keep
    // their state. Looks at most twice a second.
    void ReloadChanged();

    // What a script declares (cached until its source changes).
    const ScriptInfo& Describe(const AssetHandle<ScriptAsset>& script);

    // Why an entity's script stopped (empty while it runs, or when the game isn't running).
    std::string GetError(Entity entity, size_t slot) const;
    // Script instances running.
    size_t InstanceCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
