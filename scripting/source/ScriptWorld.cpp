#include <Mariana/Scripting/ScriptWorld.hpp>

#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/GameClock.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Foundation/Profiler.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>

#include "Bindings.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>

#if MARIANA_LUAU_CODEGEN
#include "luacodegen.h"
#endif

using namespace ScriptingInternal;

namespace
{
    enum Callback
    {
        kStart, kUpdate, kLateUpdate, kOnDestroy,
        kOnContactBegin, kOnContactEnd, kOnTriggerEnter, kOnTriggerExit,
        kOnAnimationEvent,
        kCallbackCount
    };
    const char* const kCallbackNames[kCallbackCount] = {
        "Start", "Update", "LateUpdate", "OnDestroy",
        "OnContactBegin", "OnContactEnd", "OnTriggerEnter", "OnTriggerExit",
        "OnAnimationEvent",
    };

    // A script file loaded into the state: the table it returned, the metatable its instances share
    // (whose __index is that table, so new code reaches every instance at once) and its callbacks.
    struct Module
    {
        AssetHandle<ScriptAsset> asset;
        std::string name; // the file's name without its extension (entity:GetScript)
        uint32_t version = 0;
        int table = LUA_NOREF;
        int meta = LUA_NOREF;
        int callbacks[kCallbackCount];
        std::string error;
        bool reloaded = false; // new code since the last sync
        // What the profiler calls each callback's calls ("VehicleDriver.Update"), named on first use.
        mutable const char* scopes[kCallbackCount] = {};

        Module() { std::fill(std::begin(callbacks), std::end(callbacks), LUA_NOREF); }
        bool Loaded() const { return table != LUA_NOREF; }
    };

    // A script on an entity: its `self`.
    struct Instance
    {
        uint64_t entity = 0;
        size_t slot = 0;
        AssetID script;
        int self = LUA_NOREF;
        bool enabled = true;
        bool started = false;
        bool failed = false;
        bool seen = false;
        std::vector<ScriptValue> applied; // the entity's values as last put into self
        std::string error;
    };

    bool Before(const Instance& a, const Instance& b)
    {
        return a.entity != b.entity ? a.entity < b.entity : a.slot < b.slot;
    }

    bool Same(const ScriptValue& a, const ScriptValue& b)
    {
        if (a.name != b.name || a.type != b.type)
            return false;
        switch (a.type)
        {
        case ScriptValueType::Number: return a.number == b.number;
        case ScriptValueType::Bool: return a.boolean == b.boolean;
        case ScriptValueType::String: return a.text == b.text;
        case ScriptValueType::Vector: return a.vector == b.vector;
        }
        return false;
    }

    const ScriptValue* FindValue(const std::vector<ScriptValue>& values, std::string_view name)
    {
        for (const ScriptValue& v : values)
            if (v.name == name)
                return &v;
        return nullptr;
    }

    // The type a Luau value has as a property (false for types properties can't save).
    bool PropertyType(lua_State* L, int index, ScriptValueType& type)
    {
        switch (lua_type(L, index))
        {
        case LUA_TNUMBER: type = ScriptValueType::Number; return true;
        case LUA_TBOOLEAN: type = ScriptValueType::Bool; return true;
        case LUA_TSTRING: type = ScriptValueType::String; return true;
        case LUA_TVECTOR: type = ScriptValueType::Vector; return true;
        default: return false;
        }
    }

    void PushValue(lua_State* L, const ScriptValue& value)
    {
        switch (value.type)
        {
        case ScriptValueType::Number: lua_pushnumber(L, value.number); break;
        case ScriptValueType::Bool: lua_pushboolean(L, value.boolean); break;
        case ScriptValueType::String: lua_pushstring(L, value.text.c_str()); break;
        case ScriptValueType::Vector: PushVector(L, value.vector); break;
        }
    }

    int Traceback(lua_State* L)
    {
        const char* message = lua_tostring(L, 1);
        luaL_traceback(L, L, message ? message : "(an error that isn't a string)", 1);
        return 1;
    }

    // A script other scripts load with require: run once per state, what it returned kept until its
    // file changes.
    struct Library
    {
        AssetHandle<ScriptAsset> asset;
        uint32_t version = 0;
        int value = LUA_NOREF;
        bool loading = false;
        std::vector<std::string> users; // resource paths of the scripts that required it
    };

    // A state's libraries, and the resource path of each script running in it (by its globals, which
    // its functions share), for require's relative paths.
    struct Libraries
    {
        std::unordered_map<std::string, Library> byPath;
        int scriptPaths = LUA_NOREF; // weak-keyed table: a script's globals -> its resource path
    };

    std::string ResourceOf(const ScriptAsset& asset)
    {
        return asset.file.empty() ? std::string() : FileSystem::ToResourcePath(asset.file);
    }

    std::string StemOf(const AssetHandle<ScriptAsset>& handle)
    {
        const std::shared_ptr<ScriptAsset>& asset = handle.Get();
        if (asset && !asset->file.empty())
            return asset->file.stem().string();
        const AssetMetadata* meta = handle.GetMetadata();
        return meta ? std::filesystem::path(meta->name).stem().string() : std::string("script");
    }
}

struct ScriptWorld::Impl
{
    Scene& scene;
    AssetManager2& assets;
    GameClock& clock;
    Context context;
    lua_State* L = nullptr;
    std::unordered_map<AssetID, Module> modules;
    std::vector<Instance> instances; // by entity id, then slot
    Libraries libraries;

    // Reading what scripts declare, apart from the game (no scene, input or physics).
    Context inspectContext;
    lua_State* inspect = nullptr;
    Libraries inspectLibraries;
    struct Described
    {
        bool read = false;
        uint32_t version = 0;
        ScriptInfo info;
    };
    std::unordered_map<AssetID, Described> described;
    std::chrono::steady_clock::time_point lastReloadCheck{};

    Impl(Scene& scene, AssetManager2& assets, GameClock& clock) : scene(scene), assets(assets), clock(clock)
    {
        context.scene = &scene;
        context.assets = &assets;
        context.clock = &clock;
        context.game = true;
        context.pushScript = [this](lua_State* state, Entity entity, std::string_view name) { return PushScript(state, entity, name); };
        context.require = [this](lua_State* state) { return Require(state, context, libraries); };
        inspectContext = context;
        inspectContext.game = false;
        inspectContext.pushScript = nullptr;
        inspectContext.require = [this](lua_State* state) { return Require(state, inspectContext, inspectLibraries); };
    }

    ~Impl()
    {
        Close();
        if (inspect)
            lua_close(inspect);
    }

    void Close()
    {
        if (L)
            lua_close(L);
        L = nullptr;
        modules.clear();
        instances.clear();
        context.destroys.clear();
        context.entityCache = LUA_NOREF;
        // After the state: its userdata let go of their meshes and materials as it closed.
        context.scriptMaterials.clear();
        context.scriptMeshes.clear();
        libraries = {};
    }

    // ---- Calls ----------------------------------------------------------------------------------

    // Runs the function on top of the stack (with its arguments above it) protected and with the
    // time limit; false with the error (and its traceback) in `error`.
    static bool Run(lua_State* state, Context& ctx, int nargs, int nresults, std::string& error)
    {
        const int function = lua_gettop(state) - nargs;
        lua_pushcfunction(state, Traceback, "traceback");
        lua_insert(state, function);
        // A run inside another (a script required while one runs) keeps the outer one's deadline.
        const double outer = ctx.deadline;
        if (outer <= 0.0)
        {
            ctx.deadline = lua_clock() + ctx.timeLimit;
            ctx.interrupts = 0;
        }
        const int status = lua_pcall(state, nargs, nresults, function);
        ctx.deadline = outer;
        if (status != LUA_OK)
        {
            const char* message = lua_tostring(state, -1);
            error = message ? message : "error";
            lua_pop(state, 2); // the error and the handler
            return false;
        }
        lua_remove(state, function); // the handler
        return true;
    }

    // An instance runs while its entity and every entity above it are active.
    bool Active(const Instance& instance) const
    {
        return scene.FromId(instance.entity).IsActive();
    }

    const Module* ModuleOf(const Instance& instance) const
    {
        const auto it = modules.find(instance.script);
        return it == modules.end() ? nullptr : &it->second;
    }

    void Fail(Instance& instance, const Module& module, const std::string& error)
    {
        instance.failed = true;
        instance.error = error;
        const Entity entity = scene.FromId(instance.entity);
        Logger::Error("Script " + module.name + " on '" + (entity.IsValid() ? entity.GetName() : "?") + "' stopped: " + error);
    }

    // Calls a callback of the instance with the arguments `push` puts on the stack (it returns how
    // many).
    template <typename Push>
    void Call(Instance& instance, Callback callback, Push&& push)
    {
        const Module* module = ModuleOf(instance);
        if (!module || instance.failed || instance.self == LUA_NOREF || module->callbacks[callback] == LUA_NOREF)
            return;
        if (!module->scopes[callback])
            module->scopes[callback] = Profiler::Intern(module->name + "." + kCallbackNames[callback]);
        MARIANA_PROFILE(module->scopes[callback]);
        const int top = lua_gettop(L);
        lua_getref(L, module->callbacks[callback]);
        lua_getref(L, instance.self);
        const int nargs = push(L);
        std::string error;
        if (!Run(L, context, 1 + nargs, 0, error))
            Fail(instance, *module, error);
        lua_settop(L, top);
    }

    void Call(Instance& instance, Callback callback)
    {
        Call(instance, callback, [](lua_State*) { return 0; });
    }

    // ---- Modules --------------------------------------------------------------------------------

    void EnsureState()
    {
        if (!L)
            L = NewState(context);
    }

    // Runs a script's chunk in a thread of its own (its globals, noted in `libs` as the script's)
    // and leaves what it returned on `state`'s stack (a script on an entity must return a table);
    // false with the error otherwise.
    static bool RunChunk(lua_State* state, Context& ctx, const ScriptAsset& asset, const std::string& name, std::string& error, Libraries& libs,
                         bool library = false)
    {
        if (!asset.error.empty())
        {
            error = asset.error;
            return false;
        }
        if (asset.bytecode.empty())
        {
            error = name + " has no code";
            return false;
        }
        const int top = lua_gettop(state);
        lua_State* thread = lua_newthread(state);
        luaL_sandboxthread(thread);
        if (libs.scriptPaths == LUA_NOREF)
        {
            lua_newtable(state);
            lua_newtable(state);
            lua_pushstring(state, "k");
            lua_setfield(state, -2, "__mode");
            lua_setmetatable(state, -2);
            libs.scriptPaths = lua_ref(state, -1);
            lua_pop(state, 1);
        }
        lua_getref(thread, libs.scriptPaths);
        lua_pushvalue(thread, LUA_GLOBALSINDEX);
        lua_pushstring(thread, ResourceOf(asset).c_str());
        lua_rawset(thread, -3);
        lua_pop(thread, 1);
        const std::string chunk = "=" + name;
        if (luau_load(thread, chunk.c_str(), asset.bytecode.data(), asset.bytecode.size(), 0) != 0)
        {
            error = lua_tostring(thread, -1);
            lua_settop(state, top);
            return false;
        }
#if MARIANA_LUAU_CODEGEN
        if (luau_codegen_supported())
            luau_codegen_compile(thread, -1);
#endif
        if (!Run(thread, ctx, 0, 1, error))
        {
            lua_settop(state, top);
            return false;
        }
        if (!library && !lua_istable(thread, -1))
        {
            error = name + " must return a table (its functions and properties)";
            lua_settop(state, top);
            return false;
        }
        lua_xmove(thread, state, 1);
        lua_remove(state, -2); // the thread
        return true;
    }

    // The loaded module of a script, (re)loading it when its source changed. A reload that fails
    // keeps the code that ran before.
    Module& GetModule(const AssetHandle<ScriptAsset>& handle)
    {
        Module& module = modules[handle.GetID()];
        const ScriptAsset& asset = *handle.Get();
        if (module.asset.IsValid() && module.version == asset.Version())
            return module;
        const bool reload = module.asset.IsValid();
        module.asset = handle;
        module.name = StemOf(handle);
        module.version = asset.Version();

        std::string error;
        if (!RunChunk(L, context, asset, module.name, error, libraries))
        {
            module.error = error;
            Logger::Error("Script " + module.name + ": " + error);
            return module;
        }
        // The new table: its callbacks, and the instances' metatable pointing at it.
        for (int c = 0; c < kCallbackCount; ++c)
        {
            if (module.callbacks[c] != LUA_NOREF)
                lua_unref(L, module.callbacks[c]);
            module.callbacks[c] = LUA_NOREF;
            lua_getfield(L, -1, kCallbackNames[c]);
            if (lua_isfunction(L, -1))
                module.callbacks[c] = lua_ref(L, -1);
            lua_pop(L, 1);
        }
        if (module.meta == LUA_NOREF)
        {
            lua_newtable(L);
            module.meta = lua_ref(L, -1);
            lua_pop(L, 1);
        }
        lua_getref(L, module.meta);
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, "__index");
        lua_pop(L, 1);
        if (module.table != LUA_NOREF)
            lua_unref(L, module.table);
        module.table = lua_ref(L, -1);
        lua_pop(L, 1);
        module.error.clear();
        module.reloaded = reload;
        if (reload)
            Logger::Info("Script " + module.name + " reloaded");
        return module;
    }

    // ---- require --------------------------------------------------------------------------------

    // require(name): what the script named returned, running it the first time (and again after its
    // file changed). Relative names are from the folder of the script that calls it.
    int Require(lua_State* state, Context& ctx, Libraries& libs)
    {
        const std::string name = luaL_checkstring(state, 1);
        std::string from;
        lua_Debug ar;
        if (libs.scriptPaths != LUA_NOREF && lua_getinfo(state, 1, "f", &ar))
        {
            lua_getfenv(state, -1);
            lua_getref(state, libs.scriptPaths);
            lua_pushvalue(state, -2);
            lua_rawget(state, -2);
            if (const char* path = lua_tostring(state, -1))
                from = path;
            lua_pop(state, 4); // path, table, globals, function
        }
        const std::string resource = ScriptAsset::ResolveRequire(name, from);
        if (resource.empty())
        {
            const std::string message = "require: there is no script " + name + (from.empty() ? std::string() : " (from " + from + ")");
            Error(state, "%s", message.c_str());
        }
        AssetHandle<ScriptAsset> handle = assets.Load<ScriptAsset>(FileSystem::Resolve(resource));
        if (!handle.Get())
            Error(state, "require: %s could not be read", resource.c_str());
        Library& library = libs.byPath[resource];
        if (std::find(library.users.begin(), library.users.end(), from) == library.users.end())
            library.users.push_back(from);
        if (library.value != LUA_NOREF && library.version == handle.Get()->Version())
        {
            lua_getref(state, library.value);
            return 1;
        }
        if (library.loading)
            Error(state, "require: %s requires itself (through %s)", resource.c_str(), from.c_str());
        library.loading = true;
        std::string error;
        const bool ran = RunChunk(state, ctx, *handle.Get(), StemOf(handle), error, libs, true);
        Library& again = libs.byPath[resource]; // the chunk may have required others
        again.loading = false;
        if (!ran)
        {
            const std::string message = "require " + resource + ": " + error;
            Error(state, "%s", message.c_str());
        }
        if (again.value != LUA_NOREF)
            lua_unref(state, again.value);
        again.value = lua_ref(state, -1);
        again.asset = handle;
        again.version = handle.Get()->Version();
        return 1;
    }

    // Libraries whose files changed run again the next time they're required, and so does every
    // script that required them (a module loaded again keeps its instances' state).
    void ReloadLibraries()
    {
        std::vector<std::string> changed;
        for (auto& [path, library] : libraries.byPath)
            if (library.value != LUA_NOREF && library.asset.Get() && library.asset.Get()->Version() != library.version)
                changed.push_back(path);
        std::unordered_set<std::string> stale;
        while (!changed.empty())
        {
            const std::string path = changed.back();
            changed.pop_back();
            if (!stale.insert(path).second)
                continue;
            const auto found = libraries.byPath.find(path);
            if (found == libraries.byPath.end())
                continue;
            if (found->second.value != LUA_NOREF)
                lua_unref(L, found->second.value);
            found->second.value = LUA_NOREF;
            for (const std::string& user : found->second.users)
                changed.push_back(user);
        }
        if (stale.empty())
            return;
        for (auto& [id, module] : modules)
            if (module.asset.Get() && stale.count(ResourceOf(*module.asset.Get())))
                module.version = ~0u; // loaded again by the next GetModule
    }

    // Puts the module's properties into the table on top of the stack: the entity's value where it
    // has one of the property's type, else the default (tables copied); only those `want(name)` picks.
    template <typename Want>
    void ApplyProperties(const Module& module, const std::vector<ScriptValue>& values, Want&& want)
    {
        const int self = lua_gettop(L);
        lua_getref(L, module.table);
        lua_getfield(L, -1, "properties");
        if (!lua_istable(L, -1))
        {
            lua_settop(L, self);
            return;
        }
        const int properties = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, properties) != 0)
        {
            // key at -2, default at -1
            if (lua_type(L, -2) == LUA_TSTRING)
            {
                const char* name = lua_tostring(L, -2);
                if (want(name))
                {
                    ScriptValueType type;
                    const ScriptValue* value = FindValue(values, name);
                    if (value && PropertyType(L, -1, type) && type == value->type)
                        PushValue(L, *value);
                    else if (lua_istable(L, -1))
                        lua_clonetable(L, -1); // each instance its own
                    else
                        lua_pushvalue(L, -1);
                    lua_rawsetfield(L, self, name);
                }
            }
            lua_pop(L, 1);
        }
        lua_settop(L, self);
    }

    bool MakeSelf(Instance& instance, const Module& module, const ScriptSlot& slot)
    {
        if (!module.Loaded())
            return false;
        lua_newtable(L);
        lua_getref(L, module.meta);
        lua_setmetatable(L, -2);
        PushEntity(L, scene.FromId(instance.entity));
        lua_setfield(L, -2, "entity");
        ApplyProperties(module, slot.values, [](const char*) { return true; });
        instance.self = lua_ref(L, -1);
        lua_pop(L, 1);
        instance.applied = slot.values;
        return true;
    }

    // ---- Instances ------------------------------------------------------------------------------

    // Brings the instances in line with the Scripts components: new ones made (not started yet),
    // gone ones ended (OnDestroy), new code and edited values taken in.
    void Sync()
    {
        std::vector<uint64_t> ids;
        scene.Each<Scripts>([&](Entity e, const Scripts&) { ids.push_back(e.RawId()); });
        if (ids.empty() && !L)
            return; // nothing to run yet
        std::sort(ids.begin(), ids.end());
        EnsureState();
        ReloadLibraries();

        // The modules first: their chunks are script code, which may change the scene.
        std::vector<AssetHandle<ScriptAsset>> used;
        for (uint64_t id : ids)
            if (const Entity entity = scene.FromId(id); entity.IsValid())
                if (const Scripts* s = entity.TryGet<Scripts>())
                    for (const ScriptSlot& slot : s->scripts)
                        if (slot.script.IsValid() && slot.script.Get())
                            used.push_back(slot.script);
        for (const AssetHandle<ScriptAsset>& script : used)
            GetModule(script);

        // Then the instances (no script code runs here).
        for (Instance& instance : instances)
            instance.seen = false;
        std::vector<Instance> added;
        for (uint64_t id : ids)
        {
            const Entity entity = scene.FromId(id);
            const Scripts* s = entity.IsValid() ? entity.TryGet<Scripts>() : nullptr;
            if (!s)
                continue;
            for (size_t i = 0; i < s->scripts.size(); ++i)
            {
                const ScriptSlot& slot = s->scripts[i];
                if (!slot.script.IsValid() || !slot.script.Get())
                    continue;
                const AssetID script = slot.script.GetID();
                Module& module = modules[script];
                Instance probe;
                probe.entity = id;
                probe.slot = i;
                auto it = std::lower_bound(instances.begin(), instances.end(), probe, Before);
                Instance* instance = it != instances.end() && it->entity == id && it->slot == i && it->script == script ? &*it : nullptr;
                if (!instance)
                {
                    Instance fresh;
                    fresh.entity = id;
                    fresh.slot = i;
                    fresh.script = script;
                    fresh.enabled = slot.enabled;
                    fresh.seen = true;
                    if (!MakeSelf(fresh, module, slot))
                    {
                        fresh.failed = true;
                        fresh.error = module.error;
                    }
                    added.push_back(std::move(fresh));
                    continue;
                }
                instance->seen = true;
                instance->enabled = slot.enabled;
                if (instance->self == LUA_NOREF)
                {
                    // Its script didn't load before; it may now.
                    if (MakeSelf(*instance, module, slot))
                    {
                        instance->failed = false;
                        instance->error.clear();
                    }
                    else
                        instance->error = module.error;
                    continue;
                }
                if (module.reloaded)
                {
                    // New code: errors forgotten, properties it didn't have before added.
                    instance->failed = false;
                    instance->error.clear();
                    lua_getref(L, instance->self);
                    const int self = lua_gettop(L);
                    ApplyProperties(module, slot.values, [&](const char* name) {
                        lua_rawgetfield(L, self, name);
                        const bool missing = lua_isnil(L, -1);
                        lua_pop(L, 1);
                        return missing;
                    });
                    lua_pop(L, 1);
                }
                // Values edited while it runs (the inspector).
                const std::vector<ScriptValue>& values = slot.values;
                bool changed = values.size() != instance->applied.size();
                for (size_t v = 0; !changed && v < values.size(); ++v)
                    changed = !Same(values[v], instance->applied[v]);
                if (changed)
                {
                    const std::vector<ScriptValue> before = instance->applied;
                    lua_getref(L, instance->self);
                    ApplyProperties(module, values, [&](const char* name) {
                        const ScriptValue* a = FindValue(before, name);
                        const ScriptValue* b = FindValue(values, name);
                        return (a == nullptr) != (b == nullptr) || (a && !Same(*a, *b));
                    });
                    lua_pop(L, 1);
                    instance->applied = values;
                }
            }
        }
        for (auto& [id, module] : modules)
            module.reloaded = false;

        // Instances whose entity, slot or script went: ended, then dropped.
        std::vector<Instance> gone;
        for (Instance& instance : instances)
            if (!instance.seen)
                gone.push_back(instance);
        if (!gone.empty())
            instances.erase(std::remove_if(instances.begin(), instances.end(), [](const Instance& i) { return !i.seen; }), instances.end());
        if (!added.empty())
        {
            instances.insert(instances.end(), std::make_move_iterator(added.begin()), std::make_move_iterator(added.end()));
            std::sort(instances.begin(), instances.end(), Before);
        }
        for (Instance& instance : gone)
            End(instance);
    }

    void End(Instance& instance)
    {
        if (instance.started)
            Call(instance, kOnDestroy);
        if (instance.self != LUA_NOREF)
            lua_unref(L, instance.self);
        instance.self = LUA_NOREF;
    }

    // The instances on an entity: [first, last).
    std::pair<size_t, size_t> InstancesOf(uint64_t entity) const
    {
        Instance probe;
        probe.entity = entity;
        const auto first = std::lower_bound(instances.begin(), instances.end(), probe, Before);
        auto last = first;
        while (last != instances.end() && last->entity == entity)
            ++last;
        return { static_cast<size_t>(first - instances.begin()), static_cast<size_t>(last - instances.begin()) };
    }

    // Destroys what scripts asked to, ending the scripts on those entities and below them first.
    void FlushDestroys()
    {
        for (int round = 0; round < 8 && !context.destroys.empty(); ++round)
        {
            std::vector<Entity> destroys;
            destroys.swap(context.destroys);
            for (Entity entity : destroys)
            {
                if (!entity.IsValid())
                    continue;
                std::vector<uint64_t> doomed{ entity.RawId() };
                scene.ForEachDescendant(entity, [&](Entity e) { doomed.push_back(e.RawId()); });
                std::sort(doomed.begin(), doomed.end());
                std::vector<Instance> ending;
                for (Instance& instance : instances)
                    if (std::binary_search(doomed.begin(), doomed.end(), instance.entity))
                        ending.push_back(instance);
                instances.erase(std::remove_if(instances.begin(), instances.end(),
                                               [&](const Instance& i) { return std::binary_search(doomed.begin(), doomed.end(), i.entity); }),
                                instances.end());
                for (Instance& instance : ending)
                    End(instance);
                if (entity.IsValid())
                    scene.Destroy(entity);
            }
        }
        context.destroys.clear();
    }

    bool PushScript(lua_State* state, Entity entity, std::string_view name)
    {
        const auto [first, last] = InstancesOf(entity.RawId());
        for (size_t i = first; i < last; ++i)
        {
            const Module* module = ModuleOf(instances[i]);
            if (module && module->name == name && instances[i].self != LUA_NOREF)
            {
                lua_getref(state, instances[i].self);
                return true;
            }
        }
        return false;
    }

    // ---- Contacts -------------------------------------------------------------------------------

    void Contact(uint64_t entity, Entity other, Callback callback, const ContactEvent* event, float normalSign)
    {
        const auto [first, last] = InstancesOf(entity);
        for (size_t i = first; i < last && i < instances.size(); ++i)
        {
            Instance& instance = instances[i];
            if (!instance.enabled || !instance.started || !Active(instance))
                continue;
            Call(instance, callback, [&](lua_State* state) {
                PushEntity(state, other);
                if (!event)
                    return 1;
                lua_createtable(state, 0, 3);
                PushVector(state, event->point);
                lua_setfield(state, -2, "point");
                PushVector(state, event->normal * normalSign);
                lua_setfield(state, -2, "normal");
                lua_pushnumber(state, event->speed);
                lua_setfield(state, -2, "speed");
                return 2;
            });
        }
    }

    void DispatchContacts()
    {
        if (!context.physics || instances.empty())
            return;
        // A copy: callbacks may step nothing, but they may change what the vector's entities are.
        const std::vector<ContactEvent> events = context.physics->Events();
        for (const ContactEvent& event : events)
        {
            const bool begin = event.type == ContactEvent::Type::Begin;
            const Callback callback = event.trigger ? (begin ? kOnTriggerEnter : kOnTriggerExit) : (begin ? kOnContactBegin : kOnContactEnd);
            const bool details = begin && !event.trigger;
            // The normal points from a to b: each side gets it pointing away from itself.
            Contact(event.a.RawId(), event.b, callback, details ? &event : nullptr, 1.0f);
            Contact(event.b.RawId(), event.a, callback, details ? &event : nullptr, -1.0f);
        }
    }

    // ---- Describe -------------------------------------------------------------------------------

    ScriptInfo Read(const ScriptAsset& asset, const std::string& name)
    {
        ScriptInfo info;
        if (!inspect)
            inspect = NewState(inspectContext);
        const int top = lua_gettop(inspect);
        if (!RunChunk(inspect, inspectContext, asset, name, info.error, inspectLibraries))
            return info;
        lua_getfield(inspect, -1, "properties");
        if (lua_istable(inspect, -1))
        {
            lua_pushnil(inspect);
            while (lua_next(inspect, -2) != 0)
            {
                ScriptValue value;
                if (lua_type(inspect, -2) == LUA_TSTRING && PropertyType(inspect, -1, value.type))
                {
                    value.name = lua_tostring(inspect, -2);
                    switch (value.type)
                    {
                    case ScriptValueType::Number: value.number = static_cast<float>(lua_tonumber(inspect, -1)); break;
                    case ScriptValueType::Bool: value.boolean = lua_toboolean(inspect, -1) != 0; break;
                    case ScriptValueType::String: value.text = lua_tostring(inspect, -1); break;
                    case ScriptValueType::Vector:
                    {
                        const float* v = lua_tovector(inspect, -1);
                        value.vector = { v[0], v[1], v[2] };
                        break;
                    }
                    }
                    info.properties.push_back(std::move(value));
                }
                lua_pop(inspect, 1);
            }
        }
        lua_settop(inspect, top);
        std::sort(info.properties.begin(), info.properties.end(), [](const ScriptValue& a, const ScriptValue& b) { return a.name < b.name; });
        return info;
    }
};

ScriptWorld::ScriptWorld(Scene& scene, AssetManager2& assets, GameClock& clock)
    : m_Impl(std::make_unique<Impl>(scene, assets, clock))
{
}

ScriptWorld::~ScriptWorld() = default;

void ScriptWorld::SetPhysics(PhysicsWorld* physics)
{
    m_Impl->context.physics = physics;
}

void ScriptWorld::SetTimeLimit(double seconds)
{
    m_Impl->context.timeLimit = seconds;
    m_Impl->inspectContext.timeLimit = seconds;
}

void ScriptWorld::Update(float dt, const IInput* input)
{
    MARIANA_PROFILE("Scripts");
    Impl& w = *m_Impl;
    w.context.input = input;
    w.Sync();
    Profiler::Count("Script instances", static_cast<double>(w.instances.size()));
    if (!w.L)
        return;
    // Every new script starts before any updates.
    for (size_t i = 0; i < w.instances.size(); ++i)
    {
        Instance& instance = w.instances[i];
        if (instance.enabled && !instance.started && !instance.failed && instance.self != LUA_NOREF && w.Active(instance))
        {
            instance.started = true;
            w.Call(instance, kStart);
        }
    }
    for (size_t i = 0; i < w.instances.size(); ++i)
    {
        Instance& instance = w.instances[i];
        if (instance.enabled && instance.started && w.Active(instance))
            w.Call(instance, kUpdate, [dt](lua_State* L) {
                lua_pushnumber(L, dt);
                return 1;
            });
    }
    w.FlushDestroys();
}

void ScriptWorld::LateUpdate(float dt)
{
    MARIANA_PROFILE("Scripts late");
    Impl& w = *m_Impl;
    if (!w.L)
        return;
    w.DispatchContacts();
    // The events the entities' Animators passed this frame (instances are in entity id order).
    for (size_t i = 0; i < w.instances.size(); ++i)
    {
        const Entity e = w.scene.FromId(w.instances[i].entity);
        const Animator* animator = e.IsValid() ? e.TryGet<Animator>() : nullptr;
        if (!animator || animator->Events().empty())
            continue;
        const std::vector<Animator::FiredEvent> events = animator->Events();
        for (const Animator::FiredEvent& event : events)
        {
            Instance& instance = w.instances[i];
            if (instance.enabled && instance.started && w.Active(instance))
                w.Call(instance, kOnAnimationEvent, [&event](lua_State* L) {
                    lua_pushstring(L, event.name.c_str());
                    lua_pushstring(L, event.clip.c_str());
                    return 2;
                });
        }
    }
    for (size_t i = 0; i < w.instances.size(); ++i)
    {
        Instance& instance = w.instances[i];
        if (instance.enabled && instance.started && w.Active(instance))
            w.Call(instance, kLateUpdate, [dt](lua_State* L) {
                lua_pushnumber(L, dt);
                return 1;
            });
    }
    w.FlushDestroys();
}

void ScriptWorld::Reset()
{
    Impl& w = *m_Impl;
    if (!w.L)
        return;
    for (Instance& instance : w.instances)
        if (instance.started)
            w.Call(instance, kOnDestroy);
    w.Close();
}

void ScriptWorld::ReloadChanged()
{
    Impl& w = *m_Impl;
    const auto now = std::chrono::steady_clock::now();
    if (now - w.lastReloadCheck < std::chrono::milliseconds(500))
        return;
    MARIANA_PROFILE("Script reload");
    w.lastReloadCheck = now;
    w.assets.ForEach<ScriptAsset>([](AssetID, ScriptAsset& script) {
        if (script.ReloadIfChanged() && !script.error.empty())
            Logger::Error(script.error);
    });
}

const ScriptInfo& ScriptWorld::Describe(const AssetHandle<ScriptAsset>& script)
{
    Impl& w = *m_Impl;
    static const ScriptInfo none{ {}, "No script" };
    if (!script.IsValid() || !script.Get())
        return none;
    Impl::Described& described = w.described[script.GetID()];
    const uint32_t version = script.Get()->Version();
    if (!described.read || described.version != version)
    {
        described.read = true;
        described.version = version;
        described.info = w.Read(*script.Get(), StemOf(script));
    }
    return described.info;
}

std::string ScriptWorld::GetError(Entity entity, size_t slot) const
{
    const Impl& w = *m_Impl;
    const auto [first, last] = w.InstancesOf(entity.RawId());
    for (size_t i = first; i < last; ++i)
        if (w.instances[i].slot == slot)
            return w.instances[i].error;
    return {};
}

size_t ScriptWorld::InstanceCount() const
{
    return m_Impl->instances.size();
}
