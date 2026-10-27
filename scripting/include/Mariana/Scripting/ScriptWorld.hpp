#pragma once
#include <Mariana/Scripting/Script.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <memory>
#include <string>
#include <vector>

class AssetManager2;
class GameClock;
class IInput;
class PhysicsWorld;

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

    // Physics for scripts (queries, forces, contact callbacks); none by default.
    void SetPhysics(PhysicsWorld* physics);
    // How long one call into a script may run before it is stopped as an endless loop (2 s).
    void SetTimeLimit(double seconds);

    // Each frame the game advances, before animation and physics: starts the scripts that are new
    // (Start), then updates them (Update(dt)). Without `input` keys and buttons read as up (the
    // editor's text fields have them).
    void Update(float dt, const IInput* input);
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
