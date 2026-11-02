#pragma once
// What the Luau bindings share (private to MarianaScripting): the state's context, the userdata
// types and the conversions between Luau values and engine types.
#include <Mariana/Assets/AssetsStructs.hpp>
#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Scene/Transform.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lua.h"
#include "lualib.h"

class AssetManager2;
class GameClock;
class IInput;
class IMesh;
class PhysicsWorld;

namespace ScriptingInternal
{
    // Userdata tags, each with its metatable (lua_setuserdatametatable).
    enum Tag : int
    {
        kEntityTag = 1,
        kComponentTag,
        kQuatTag,
        kTimeTag,
        kMeshTag,
        kMaterialTag,
        kTextureTag,
    };

    struct ScriptMesh; // AssetBindings.cpp

    // What the bindings work with, reached from any thread of the state (lua_callbacks userdata).
    struct Context
    {
        Scene* scene = nullptr;
        AssetManager2* assets = nullptr;
        GameClock* clock = nullptr;
        PhysicsWorld* physics = nullptr;
        const IInput* input = nullptr;
        // The game's libraries (scene, input, physics); the editor's state for reading properties
        // has only the pure ones.
        bool game = false;
        // Destroys asked for while scripts run, done once they have all run (ScriptWorld).
        std::vector<Entity> destroys;
        // An entity's script instance by the script's name (entity:GetScript); pushes nothing and
        // returns false when there is none.
        std::function<bool(lua_State*, Entity, std::string_view)> pushScript;
        // A running call stops with an error once this passes (an endless loop); 0 = no limit.
        double deadline = 0.0;
        double timeLimit = 2.0; // seconds a call may take
        uint32_t interrupts = 0;
        int entityCache = LUA_NOREF; // weak table: entity id -> its userdata
        // Materials scripts made (material.new, Clone) or were given a copy of (entity:GetMaterial):
        // theirs to change, so changing them never reaches a material the edited scene or a model
        // shares. Meshes scripts made, by their engine mesh (what entity:GetMesh finds again).
        std::unordered_set<AssetID> scriptMaterials;
        std::unordered_map<const IMesh*, std::shared_ptr<ScriptMesh>> scriptMeshes;
    };
    Context& GetContext(lua_State* L);

    // A state with the libraries scripts see (vec3, quat, Entity, and with `context.game` scene,
    // input, time and physics), sandboxed: globals are read-only and each module gets its own.
    lua_State* NewState(Context& context);

    // Values.
    void PushVector(lua_State* L, const glm::vec3& v);
    glm::vec3 CheckVector(lua_State* L, int index);
    glm::vec3 OptVector(lua_State* L, int index, const glm::vec3& fallback);
    void PushQuat(lua_State* L, const glm::quat& q);
    glm::quat CheckQuat(lua_State* L, int index);
    bool IsQuat(lua_State* L, int index);
    // The same userdata for an entity while Luau holds on to it (so entities work as table keys);
    // nil for an invalid one.
    void PushEntity(lua_State* L, Entity entity);
    Entity CheckEntity(lua_State* L, int index); // a valid entity, or an error
    Entity ToEntity(lua_State* L, int index);    // whatever the value names (maybe invalid)

    // Transforms through the LocalTRS chain (current even before Scene::Update has run; the matrix is
    // the engine's WorldMatrix, Mariana/Scene/Transform.hpp).
    glm::quat WorldRotation(const Scene& scene, Entity entity);

    // Names the bindings look up by number (Luau gives strings these atoms as they are made, so
    // `entity.position` and `entity:Get(...)` switch on a number).
    enum Atom : int16_t
    {
        // Entity
        kId, kName, kParent, kPosition, kRotation, kScale, kWorldPosition, kWorldRotation,
        kForward, kRight, kUp, kIsValid, kDestroy, kHas, kGet, kAdd, kRemove, kGetScript,
        kGetChildren, kFindChild, kRotate, kTranslate, kLookAt, kSetMesh, kGetMesh, kGetMaterial,
        kSetMaterial,
        // quat
        kX, kY, kZ, kW, kInverse, kToEuler,
        // time
        kDelta, kTime, kFrame,
        // Animator
        kSetFloat, kSetBool, kSetTrigger, kResetTrigger, kGetFloat, kGetBool, kPlay, kCrossFade,
        kCrossFadeState, kGetRootVelocity, kSetVector, kGetVector,
        kAtomCount
    };
    int16_t AtomOf(const char* name, size_t length);
    // The atom of the string at `index` (-1 when it is none of them); its text in `name`.
    int KeyAtom(lua_State* L, int index, const char** name = nullptr);

    // The libraries, registered by NewState.
    void OpenMath(lua_State* L);
    void OpenEntity(lua_State* L);
    void OpenGame(lua_State* L);
    void OpenAssets(lua_State* L);

    // Entity methods that work with meshes and materials (AssetBindings.cpp).
    int EntitySetMesh(lua_State* L);
    int EntityGetMesh(lua_State* L);
    int EntityGetMaterial(lua_State* L);
    int EntitySetMaterial(lua_State* L);

    // An error with the message, raised from a C function.
    [[noreturn]] void Error(lua_State* L, const char* fmt, ...);
}
