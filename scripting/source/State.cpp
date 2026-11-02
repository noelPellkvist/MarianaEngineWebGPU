// The Luau state scripts run in: its context, sandbox, print, the endless-loop guard, and the
// conversions every binding uses.
#include "Bindings.hpp"

#include <Mariana/Foundation/Logger.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include "Luau/Common.h"

#include <cstdarg>
#include <cstdio>
#include <string>

#if MARIANA_LUAU_CODEGEN
#include "luacodegen.h"
#endif

namespace ScriptingInternal
{
    Context& GetContext(lua_State* L)
    {
        return *static_cast<Context*>(lua_callbacks(L)->userdata);
    }

    void Error(lua_State* L, const char* fmt, ...)
    {
        char message[512];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(message, sizeof(message), fmt, args);
        va_end(args);
        luaL_error(L, "%s", message);
    }

    namespace
    {
        // Where a print came from: "Spin.luau:12".
        std::string Where(lua_State* L)
        {
            lua_Debug ar;
            for (int level = 1; lua_getinfo(L, level, "sl", &ar); ++level)
                if (ar.what && std::string(ar.what) != "C")
                    return std::string(ar.short_src) + ":" + std::to_string(ar.currentline);
            return "script";
        }

        std::string Join(lua_State* L)
        {
            std::string text;
            const int n = lua_gettop(L);
            for (int i = 1; i <= n; ++i)
            {
                size_t length = 0;
                const char* s = luaL_tolstring(L, i, &length);
                if (i > 1)
                    text += ' ';
                text.append(s, length);
                lua_pop(L, 1);
            }
            return text;
        }

        int Print(lua_State* L)
        {
            const std::string text = Join(L);
            Logger::Info("[" + Where(L) + "] " + text);
            return 0;
        }

        int Require(lua_State* L)
        {
            Context& context = GetContext(L);
            if (!context.require)
                Error(L, "require isn't available here");
            return context.require(L);
        }

        int Warn(lua_State* L)
        {
            const std::string text = Join(L);
            Logger::Warning("[" + Where(L) + "] " + text);
            return 0;
        }

        // At loop back edges and calls: a call that runs past its deadline is stopped (an endless
        // loop would otherwise hang the app). The clock is read every 1024 interrupts.
        void Interrupt(lua_State* L, int gc)
        {
            if (gc >= 0)
                return;
            Context& context = GetContext(L);
            if (context.deadline <= 0.0 || (++context.interrupts & 1023u) != 0)
                return;
            if (lua_clock() > context.deadline)
            {
                context.deadline = 0.0;
                luaL_error(L, "the script ran too long (an endless loop?)");
            }
        }

        int16_t UserAtom(lua_State*, const char* s, size_t length);
    }

    lua_State* NewState(Context& context)
    {
        // Luau checks its own invariants in debug builds: say which one failed before it breaks.
        Luau::assertHandler() = [](const char* expression, const char* file, int line, const char*) {
            Logger::Error(std::string("Luau assertion failed: ") + expression + " (" + file + ":" + std::to_string(line) + ")");
            std::fflush(nullptr);
            return 1;
        };
        lua_State* L = luaL_newstate();
        lua_Callbacks* callbacks = lua_callbacks(L);
        callbacks->userdata = &context;
        callbacks->interrupt = Interrupt;
        callbacks->useratom = UserAtom;
#if MARIANA_LUAU_CODEGEN
        if (luau_codegen_supported())
            luau_codegen_create(L);
#endif
        luaL_openlibs(L);
        lua_pushcfunction(L, Print, "print");
        lua_setglobal(L, "print");
        lua_pushcfunction(L, Warn, "warn");
        lua_setglobal(L, "warn");
        lua_pushcfunction(L, Require, "require");
        lua_setglobal(L, "require");

        // Entity userdata by id, weak so it goes when scripts drop it.
        lua_newtable(L);
        lua_newtable(L);
        lua_pushstring(L, "v");
        lua_setfield(L, -2, "__mode");
        lua_setmetatable(L, -2);
        context.entityCache = lua_ref(L, -1);
        lua_pop(L, 1);

        OpenMath(L);
        OpenEntity(L);
        if (context.game)
        {
            OpenGame(L);
            OpenAssets(L);
        }

        // Globals and libraries read-only; each module gets globals of its own.
        luaL_sandbox(L);
        return L;
    }

    // ---- Values ---------------------------------------------------------------------------------

    void PushVector(lua_State* L, const glm::vec3& v)
    {
        lua_pushvector(L, v.x, v.y, v.z);
    }

    glm::vec3 CheckVector(lua_State* L, int index)
    {
        const float* v = luaL_checkvector(L, index);
        return { v[0], v[1], v[2] };
    }

    glm::vec3 OptVector(lua_State* L, int index, const glm::vec3& fallback)
    {
        return lua_isnoneornil(L, index) ? fallback : CheckVector(L, index);
    }

    void PushEntity(lua_State* L, Entity entity)
    {
        if (!entity.IsValid())
        {
            lua_pushnil(L);
            return;
        }
        Context& context = GetContext(L);
        void* key = reinterpret_cast<void*>(static_cast<uintptr_t>(entity.RawId()));
        lua_getref(L, context.entityCache);
        lua_pushlightuserdata(L, key);
        if (lua_rawget(L, -2) == LUA_TUSERDATA)
        {
            lua_remove(L, -2);
            return;
        }
        lua_pop(L, 1);
        auto* id = static_cast<uint64_t*>(lua_newuserdatataggedwithmetatable(L, sizeof(uint64_t), kEntityTag));
        *id = entity.RawId();
        lua_pushlightuserdata(L, key);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
        lua_remove(L, -2);
    }

    Entity ToEntity(lua_State* L, int index)
    {
        const auto* id = static_cast<const uint64_t*>(lua_touserdatatagged(L, index, kEntityTag));
        return id ? GetContext(L).scene->FromId(*id) : Entity{};
    }

    Entity CheckEntity(lua_State* L, int index)
    {
        const auto* id = static_cast<const uint64_t*>(lua_touserdatatagged(L, index, kEntityTag));
        if (!id)
            luaL_typeerror(L, index, "Entity");
        const Entity entity = GetContext(L).scene->FromId(*id);
        if (!entity.IsValid())
            luaL_argerror(L, index, "the entity was destroyed");
        return entity;
    }

    // ---- Transforms -----------------------------------------------------------------------------

    glm::quat WorldRotation(const Scene& scene, Entity entity)
    {
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        for (Entity e = entity; e.IsValid(); e = scene.Parent(e))
            if (const LocalTRS* t = e.TryGet<LocalTRS>())
                rotation = glm::quat(t->rot_quat[3], t->rot_quat[0], t->rot_quat[1], t->rot_quat[2]) * rotation;
        return glm::normalize(rotation);
    }

    // ---- Atoms ----------------------------------------------------------------------------------

    namespace
    {
        const char* const kAtomNames[kAtomCount] = {
            "id", "name", "parent", "position", "rotation", "scale", "worldPosition", "worldRotation",
            "forward", "right", "up", "IsValid", "Destroy", "Has", "Get", "Add", "Remove", "GetScript",
            "GetChildren", "FindChild", "Rotate", "Translate", "LookAt", "SetMesh", "GetMesh", "GetMaterial",
            "SetMaterial", "active", "visible", "activeInHierarchy", "AddScript", "RemoveScript", "SetScriptEnabled",
            "x", "y", "z", "w", "Inverse", "ToEuler",
            "delta", "time", "frame",
            "SetFloat", "SetBool", "SetTrigger", "ResetTrigger", "GetFloat", "GetBool", "Play", "CrossFade",
            "CrossFadeState", "GetRootVelocity", "SetVector", "GetVector",
        };

        int16_t UserAtom(lua_State*, const char* s, size_t length)
        {
            return AtomOf(s, length);
        }
    }

    int16_t AtomOf(const char* name, size_t length)
    {
        const std::string_view key(name, length);
        for (int16_t i = 0; i < kAtomCount; ++i)
            if (key == kAtomNames[i])
                return i;
        return -1;
    }

    int KeyAtom(lua_State* L, int index, const char** name)
    {
        int atom = -1;
        size_t length = 0;
        const char* key = lua_tolstringatom(L, index, &length, &atom);
        if (!key)
            luaL_typeerror(L, index, "string");
        if (name)
            *name = key;
        // Strings made before the state had its atom callback have none.
        return atom >= 0 && atom < kAtomCount ? atom : AtomOf(key, length);
    }
}
