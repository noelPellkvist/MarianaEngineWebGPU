// What scripts do with the picture and time: screen (the game view, rays from it and points on
// it), lighting (the sun, the sky, the exposure), effects (post effects' settings), gizmos (shapes
// drawn to debug) and task (functions that wait, run by ScriptWorld).
#include "Bindings.hpp"

#include <Mariana/Foundation/GameClock.hpp>
#include <Mariana/Rendering/Effect.hpp>
#include <Mariana/Rendering/ICamera.hpp>
#include <Mariana/Rendering/StandardPBR.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace ScriptingInternal
{
    namespace
    {
        void SetFunction(lua_State* L, const char* name, lua_CFunction fn)
        {
            lua_pushcfunction(L, fn, name);
            lua_setfield(L, -2, name);
        }

        // ---- screen ---------------------------------------------------------------------------

        // The mouse in the game view's pixels (from its top-left corner).
        glm::vec2 MouseInView(lua_State* L)
        {
            const Context& context = GetContext(L);
            double x = 0.0, y = 0.0;
            if (context.input)
                context.input->GetMousePosition(x, y);
            return glm::vec2(static_cast<float>(x) - context.viewport.x, static_cast<float>(y) - context.viewport.y);
        }

        // screen.Ray(point?): where a point of the game view (default: the mouse) looks: the ray's
        // origin and direction (from the camera, through the point); nil while nothing shows the game.
        int ScreenRay(lua_State* L)
        {
            const Context& context = GetContext(L);
            if (!context.hasView)
            {
                lua_pushnil(L);
                lua_pushnil(L);
                return 2;
            }
            const glm::vec2 point = lua_isnoneornil(L, 1) ? MouseInView(L) : glm::vec2(CheckVector(L, 1));
            const float width = std::max(context.viewport.z, 1.0f), height = std::max(context.viewport.w, 1.0f);
            const glm::vec2 ndc(point.x / width * 2.0f - 1.0f, 1.0f - point.y / height * 2.0f);
            const glm::mat4 inverse = glm::inverse(context.projection * context.view);
            const glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
            const glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
            const glm::vec3 direction = glm::normalize(glm::vec3(farPoint) / farPoint.w - glm::vec3(nearPoint) / nearPoint.w);
            PushVector(L, context.cameraPosition);
            PushVector(L, direction);
            return 2;
        }

        // screen.WorldToScreen(point): where a point of the world is in the game view (x, y pixels from
        // its top-left corner; z its distance from the camera), or nil behind the camera.
        int ScreenWorldToScreen(lua_State* L)
        {
            const Context& context = GetContext(L);
            const glm::vec3 point = CheckVector(L, 1);
            if (!context.hasView)
            {
                lua_pushnil(L);
                return 1;
            }
            const glm::vec4 clip = context.projection * context.view * glm::vec4(point, 1.0f);
            if (clip.w <= 1e-6f)
            {
                lua_pushnil(L);
                return 1;
            }
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            lua_pushvector(L, (ndc.x + 1.0f) * 0.5f * context.viewport.z, (1.0f - ndc.y) * 0.5f * context.viewport.w,
                           glm::distance(point, context.cameraPosition));
            return 1;
        }

        int ScreenIndex(lua_State* L)
        {
            const Context& context = GetContext(L);
            const char* key = luaL_checkstring(L, 2);
            if (std::strcmp(key, "width") == 0)
                lua_pushnumber(L, context.viewport.z);
            else if (std::strcmp(key, "height") == 0)
                lua_pushnumber(L, context.viewport.w);
            else if (std::strcmp(key, "Ray") == 0)
                lua_pushcfunction(L, ScreenRay, "Ray");
            else if (std::strcmp(key, "WorldToScreen") == 0)
                lua_pushcfunction(L, ScreenWorldToScreen, "WorldToScreen");
            else
                Error(L, "screen has no member '%s'", key);
            return 1;
        }

        // ---- lighting -------------------------------------------------------------------------

        LightingSettings& Lighting(lua_State* L)
        {
            LightingSettings* lighting = GetContext(L).lighting;
            if (!lighting)
                Error(L, "lighting isn't available here");
            return *lighting;
        }

        float* LightingNumber(LightingSettings& l, const char* key)
        {
            if (std::strcmp(key, "sunAzimuth") == 0) return &l.sunAzimuth;
            if (std::strcmp(key, "sunElevation") == 0) return &l.sunElevation;
            if (std::strcmp(key, "sunIntensity") == 0) return &l.sunIntensity;
            if (std::strcmp(key, "skyIntensity") == 0) return &l.skyIntensity;
            if (std::strcmp(key, "reflectionIntensity") == 0) return &l.reflectionIntensity;
            if (std::strcmp(key, "skyRotation") == 0) return &l.skyRotation;
            return nullptr;
        }

        int LightingIndex(lua_State* L)
        {
            const char* key = luaL_checkstring(L, 2);
            LightingSettings& lighting = Lighting(L);
            if (float* number = LightingNumber(lighting, key))
                lua_pushnumber(L, *number);
            else if (std::strcmp(key, "sunColor") == 0)
                PushVector(L, lighting.sunColor);
            else if (std::strcmp(key, "exposure") == 0)
            {
                const ICamera* camera = GetContext(L).exposureCamera;
                lua_pushnumber(L, camera ? camera->Exposure() : 1.0);
            }
            else
                Error(L, "lighting has no member '%s'", key);
            return 1;
        }

        int LightingNewIndex(lua_State* L)
        {
            const char* key = luaL_checkstring(L, 2);
            LightingSettings& lighting = Lighting(L);
            if (float* number = LightingNumber(lighting, key))
                *number = static_cast<float>(luaL_checknumber(L, 3));
            else if (std::strcmp(key, "sunColor") == 0)
                lighting.sunColor = CheckVector(L, 3);
            else if (std::strcmp(key, "exposure") == 0)
            {
                ICamera* camera = GetContext(L).exposureCamera;
                if (camera)
                    camera->SetExposure(static_cast<float>(luaL_checknumber(L, 3)));
            }
            else
                Error(L, "can't set '%s' on lighting", key);
            return 0;
        }

        // ---- effects --------------------------------------------------------------------------

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        // An effect by its name ("Bloom", "Tonemap (ACES)"), the start of it ("Tonemap") or a word of
        // it ("GTAO"), ignoring case.
        EffectInstance& FindEffect(lua_State* L, int index)
        {
            EffectStack* effects = GetContext(L).effects;
            if (!effects)
                Error(L, "effects aren't available here");
            const std::string wanted = Lower(luaL_checkstring(L, index));
            EffectInstance* starts = nullptr;
            EffectInstance* contains = nullptr;
            for (const std::unique_ptr<EffectInstance>& effect : effects->Effects())
            {
                const std::string name = Lower(effect->Name());
                if (name == wanted)
                    return *effect;
                if (!starts && name.rfind(wanted, 0) == 0)
                    starts = effect.get();
                if (!contains && name.find(wanted) != std::string::npos)
                    contains = effect.get();
            }
            if (EffectInstance* found = starts ? starts : contains)
                return *found;
            Error(L, "there is no effect '%s' in the pipeline", lua_tostring(L, index));
        }

        size_t FindParam(lua_State* L, EffectInstance& effect, int index)
        {
            const char* name = luaL_checkstring(L, index);
            for (size_t i = 0; i < effect.ParamCount(); ++i)
                if (effect.ParamInfo(i).name == name)
                    return i;
            Error(L, "%s has no parameter '%s'", effect.Name().c_str(), name);
        }

        // effects.Get(effect, parameter): a number, a boolean or a vector, as the parameter is.
        int EffectsGet(lua_State* L)
        {
            EffectInstance& effect = FindEffect(L, 1);
            const size_t i = FindParam(L, effect, 2);
            const glm::vec4& value = effect.ParamValue(i);
            switch (effect.ParamInfo(i).type)
            {
            case EffectParamType::Float:
            case EffectParamType::Int: lua_pushnumber(L, value.x); break;
            case EffectParamType::Bool: lua_pushboolean(L, value.x != 0.0f); break;
            default: lua_pushvector(L, value.x, value.y, value.z); break;
            }
            return 1;
        }

        // effects.Set(effect, parameter, value): a number, a boolean or a vector (colours too).
        int EffectsSet(lua_State* L)
        {
            EffectInstance& effect = FindEffect(L, 1);
            const size_t i = FindParam(L, effect, 2);
            glm::vec4 value = effect.ParamValue(i);
            if (lua_isboolean(L, 3))
                value.x = lua_toboolean(L, 3) ? 1.0f : 0.0f;
            else if (lua_isvector(L, 3))
            {
                const glm::vec3 v = CheckVector(L, 3);
                value = glm::vec4(v, value.w);
            }
            else
                value.x = static_cast<float>(luaL_checknumber(L, 3));
            effect.ParamValue(i) = value;
            return 0;
        }

        int EffectsSetEnabled(lua_State* L)
        {
            EffectInstance& effect = FindEffect(L, 1);
            luaL_checktype(L, 2, LUA_TBOOLEAN);
            const bool on = lua_toboolean(L, 2) != 0;
            if (effect.enabled != on)
            {
                effect.enabled = on;
                GetContext(L).effects->MarkDirty();
            }
            return 0;
        }

        int EffectsIsEnabled(lua_State* L)
        {
            lua_pushboolean(L, FindEffect(L, 1).enabled);
            return 1;
        }

        int EffectsNames(lua_State* L)
        {
            lua_newtable(L);
            if (EffectStack* effects = GetContext(L).effects)
            {
                int i = 0;
                for (const std::unique_ptr<EffectInstance>& effect : effects->Effects())
                {
                    lua_pushstring(L, effect->Name().c_str());
                    lua_rawseti(L, -2, ++i);
                }
            }
            return 1;
        }

        // ---- gizmos ---------------------------------------------------------------------------

        uint32_t Color(lua_State* L, int index, uint32_t fallback)
        {
            if (lua_isnoneornil(L, index))
                return fallback;
            const glm::vec3 c = glm::clamp(CheckVector(L, index), glm::vec3(0.0f), glm::vec3(1.0f));
            return static_cast<uint32_t>(c.r * 255.0f + 0.5f) | static_cast<uint32_t>(c.g * 255.0f + 0.5f) << 8 |
                   static_cast<uint32_t>(c.b * 255.0f + 0.5f) << 16 | 0xff000000u;
        }

        void AddShape(lua_State* L, ScriptDebugShape shape, int secondsIndex)
        {
            Context& context = GetContext(L);
            shape.until = (context.clock ? context.clock->Time() : 0.0) + std::max(0.0, luaL_optnumber(L, secondsIndex, 0.0));
            if (context.gizmos.size() < 100000)
                context.gizmos.push_back(std::move(shape));
        }

        constexpr uint32_t kWhite = 0xffffffffu;

        // gizmos.Line(from, to, color?, seconds?)
        int GizmoLine(lua_State* L)
        {
            ScriptDebugShape shape;
            shape.type = ScriptDebugShape::Type::Line;
            shape.a = CheckVector(L, 1);
            shape.b = CheckVector(L, 2);
            shape.color = Color(L, 3, kWhite);
            AddShape(L, std::move(shape), 4);
            return 0;
        }

        // gizmos.Sphere(center, radius, color?, seconds?)
        int GizmoSphere(lua_State* L)
        {
            ScriptDebugShape shape;
            shape.type = ScriptDebugShape::Type::Sphere;
            shape.a = CheckVector(L, 1);
            shape.radius = static_cast<float>(luaL_checknumber(L, 2));
            shape.color = Color(L, 3, kWhite);
            AddShape(L, std::move(shape), 4);
            return 0;
        }

        // gizmos.Box(center, size, rotation?, color?, seconds?)
        int GizmoBox(lua_State* L)
        {
            ScriptDebugShape shape;
            shape.type = ScriptDebugShape::Type::Box;
            shape.a = CheckVector(L, 1);
            shape.b = CheckVector(L, 2);
            if (!lua_isnoneornil(L, 3))
                shape.rotation = CheckQuat(L, 3);
            shape.color = Color(L, 4, kWhite);
            AddShape(L, std::move(shape), 5);
            return 0;
        }

        // gizmos.Text(position, text, color?, seconds?)
        int GizmoText(lua_State* L)
        {
            ScriptDebugShape shape;
            shape.type = ScriptDebugShape::Type::Text;
            shape.a = CheckVector(L, 1);
            size_t length = 0;
            const char* text = luaL_tolstring(L, 2, &length);
            shape.text.assign(text, length);
            lua_pop(L, 1);
            shape.color = Color(L, 3, kWhite);
            AddShape(L, std::move(shape), 4);
            return 0;
        }

        // ---- task -----------------------------------------------------------------------------

        int Hook(lua_State* L, const std::function<int(lua_State*)>& hook)
        {
            if (!hook)
                Error(L, "task isn't available here");
            return hook(L);
        }

        int TaskSpawn(lua_State* L) { return Hook(L, GetContext(L).taskSpawn); }
        int TaskDelay(lua_State* L) { return Hook(L, GetContext(L).taskDelay); }
        int TaskWait(lua_State* L) { return Hook(L, GetContext(L).taskWait); }
        int TaskCancel(lua_State* L) { return Hook(L, GetContext(L).taskCancel); }

        void Locked(lua_State* L, int tag, const char* type, lua_CFunction index, lua_CFunction newIndex, const char* global)
        {
            lua_newtable(L);
            SetFunction(L, "__index", index);
            if (newIndex)
                SetFunction(L, "__newindex", newIndex);
            lua_pushstring(L, type);
            lua_setfield(L, -2, "__type");
            lua_pushstring(L, "locked");
            lua_setfield(L, -2, "__metatable");
            lua_setuserdatametatable(L, tag);
            lua_newuserdatataggedwithmetatable(L, 1, tag);
            lua_setglobal(L, global);
        }
    }

    void OpenPlay(lua_State* L)
    {
        Locked(L, kScreenTag, "Screen", ScreenIndex, nullptr, "screen");
        Locked(L, kLightingTag, "Lighting", LightingIndex, LightingNewIndex, "lighting");

        lua_newtable(L);
        SetFunction(L, "Get", EffectsGet);
        SetFunction(L, "Set", EffectsSet);
        SetFunction(L, "SetEnabled", EffectsSetEnabled);
        SetFunction(L, "IsEnabled", EffectsIsEnabled);
        SetFunction(L, "Names", EffectsNames);
        lua_setglobal(L, "effects");

        lua_newtable(L);
        SetFunction(L, "Line", GizmoLine);
        SetFunction(L, "Sphere", GizmoSphere);
        SetFunction(L, "Box", GizmoBox);
        SetFunction(L, "Text", GizmoText);
        lua_setglobal(L, "gizmos");
    }

    void OpenTask(lua_State* L)
    {
        lua_newtable(L);
        SetFunction(L, "spawn", TaskSpawn);
        SetFunction(L, "delay", TaskDelay);
        SetFunction(L, "wait", TaskWait);
        SetFunction(L, "cancel", TaskCancel);
        lua_setglobal(L, "task");
    }
}
