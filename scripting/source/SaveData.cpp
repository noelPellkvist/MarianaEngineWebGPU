// The save library: values kept between runs of the game, by key (numbers, booleans, strings,
// vectors and tables of them), in a JSON file natively or the page's localStorage on the web.
#include "SaveData.hpp"
#include "Bindings.hpp"

#include <Mariana/Foundation/Logger.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <cstdlib>
#endif

namespace ScriptingInternal
{
    using Json = nlohmann::json;

    void SaveData::Load()
    {
        if (loaded)
            return;
        loaded = true;
        data = Json::object();
        if (location.empty())
            return;
        std::string text;
#if defined(__EMSCRIPTEN__)
        char* stored = static_cast<char*>(EM_ASM_PTR({
            try {
                var value = localStorage.getItem(UTF8ToString($0));
                if (value === null) return 0;
                var size = lengthBytesUTF8(value) + 1;
                var pointer = _malloc(size);
                stringToUTF8(value, pointer, size);
                return pointer;
            } catch (e) { return 0; }
        }, location.c_str()));
        if (stored)
        {
            text = stored;
            std::free(stored);
        }
#else
        std::ifstream stream(location, std::ios::binary);
        if (!stream)
            return;
        std::ostringstream read;
        read << stream.rdbuf();
        text = read.str();
#endif
        const Json parsed = Json::parse(text, nullptr, false);
        if (parsed.is_object())
            data = parsed;
        else if (!text.empty())
            Logger::Warning("The save data in " + location + " couldn't be read; starting without it");
    }

    void SaveData::Flush()
    {
        if (!dirty || location.empty())
            return;
        dirty = false;
        const std::string text = data.dump(2);
#if defined(__EMSCRIPTEN__)
        EM_ASM({
            try { localStorage.setItem(UTF8ToString($0), UTF8ToString($1)); } catch (e) {}
        }, location.c_str(), text.c_str());
#else
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(location).parent_path(), ec);
        std::ofstream stream(location, std::ios::binary);
        stream << text << "\n";
        if (!stream)
            Logger::Error("Could not write the save data to " + location);
#endif
    }

    namespace
    {
        SaveData& Store(lua_State* L)
        {
            SaveData* save = GetContext(L).save;
            if (!save)
                Error(L, "save isn't available here");
            save->Load();
            return *save;
        }

        // A Luau value as JSON: numbers, booleans, strings, vectors ({ "vector": [x, y, z] }) and
        // tables of them (lists as arrays, the rest by string key).
        Json ToJson(lua_State* L, int index, int depth)
        {
            index = lua_absindex(L, index);
            if (depth > 32)
                Error(L, "save: tables nested too deep");
            switch (lua_type(L, index))
            {
            case LUA_TNUMBER: return lua_tonumber(L, index);
            case LUA_TBOOLEAN: return lua_toboolean(L, index) != 0;
            case LUA_TSTRING: return std::string(lua_tostring(L, index));
            case LUA_TVECTOR:
            {
                const float* v = lua_tovector(L, index);
                return Json{ { "vector", { v[0], v[1], v[2] } } };
            }
            case LUA_TTABLE:
            {
                const int n = lua_objlen(L, index);
                int count = 0;
                lua_pushnil(L);
                while (lua_next(L, index) != 0)
                {
                    ++count;
                    lua_pop(L, 1);
                }
                if (n > 0 && count == n)
                {
                    Json list = Json::array();
                    for (int i = 1; i <= n; ++i)
                    {
                        lua_rawgeti(L, index, i);
                        list.push_back(ToJson(L, -1, depth + 1));
                        lua_pop(L, 1);
                    }
                    return list;
                }
                Json object = Json::object();
                lua_pushnil(L);
                while (lua_next(L, index) != 0)
                {
                    if (lua_type(L, -2) != LUA_TSTRING)
                        Error(L, "save: a table's keys are strings or 1, 2, 3, ... in order");
                    object[lua_tostring(L, -2)] = ToJson(L, -1, depth + 1);
                    lua_pop(L, 1);
                }
                return object;
            }
            default: Error(L, "save keeps numbers, booleans, strings, vectors and tables of them, not a %s", luaL_typename(L, index));
            }
        }

        void PushJson(lua_State* L, const Json& value)
        {
            if (value.is_number())
                lua_pushnumber(L, value.get<double>());
            else if (value.is_boolean())
                lua_pushboolean(L, value.get<bool>());
            else if (value.is_string())
                lua_pushstring(L, value.get<std::string>().c_str());
            else if (value.is_array())
            {
                lua_createtable(L, static_cast<int>(value.size()), 0);
                for (size_t i = 0; i < value.size(); ++i)
                {
                    PushJson(L, value[i]);
                    lua_rawseti(L, -2, static_cast<int>(i) + 1);
                }
            }
            else if (value.is_object() && value.size() == 1 && value.contains("vector") && value["vector"].is_array() && value["vector"].size() == 3)
                lua_pushvector(L, value["vector"][0].get<float>(), value["vector"][1].get<float>(), value["vector"][2].get<float>());
            else if (value.is_object())
            {
                lua_createtable(L, 0, static_cast<int>(value.size()));
                for (const auto& [key, item] : value.items())
                {
                    PushJson(L, item);
                    lua_setfield(L, -2, key.c_str());
                }
            }
            else
                lua_pushnil(L);
        }

        // save.Set(key, value): nil removes it.
        int SaveSet(lua_State* L)
        {
            const std::string key = luaL_checkstring(L, 1);
            SaveData& save = Store(L);
            if (lua_isnoneornil(L, 2))
                save.data.erase(key);
            else
                save.data[key] = ToJson(L, 2, 0);
            save.dirty = true;
            return 0;
        }

        // save.Get(key, default): the value, or `default` (nil) when there is none.
        int SaveGet(lua_State* L)
        {
            const std::string key = luaL_checkstring(L, 1);
            SaveData& save = Store(L);
            const auto found = save.data.find(key);
            if (found == save.data.end())
                lua_pushvalue(L, 2);
            else
                PushJson(L, *found);
            return 1;
        }

        int SaveHas(lua_State* L)
        {
            const std::string key = luaL_checkstring(L, 1);
            lua_pushboolean(L, Store(L).data.contains(key));
            return 1;
        }

        int SaveDelete(lua_State* L)
        {
            const std::string key = luaL_checkstring(L, 1);
            SaveData& save = Store(L);
            save.dirty = save.data.erase(key) > 0 || save.dirty;
            return 0;
        }

        int SaveKeys(lua_State* L)
        {
            SaveData& save = Store(L);
            lua_createtable(L, static_cast<int>(save.data.size()), 0);
            int i = 0;
            for (const auto& [key, value] : save.data.items())
            {
                lua_pushstring(L, key.c_str());
                lua_rawseti(L, -2, ++i);
            }
            return 1;
        }

        int SaveClear(lua_State* L)
        {
            SaveData& save = Store(L);
            save.data = Json::object();
            save.dirty = true;
            return 0;
        }

        int SaveFlush(lua_State* L)
        {
            Store(L).Flush();
            return 0;
        }

        void SetFunction(lua_State* L, const char* name, lua_CFunction fn)
        {
            lua_pushcfunction(L, fn, name);
            lua_setfield(L, -2, name);
        }
    }

    void OpenSave(lua_State* L)
    {
        lua_newtable(L);
        SetFunction(L, "Set", SaveSet);
        SetFunction(L, "Get", SaveGet);
        SetFunction(L, "Has", SaveHas);
        SetFunction(L, "Delete", SaveDelete);
        SetFunction(L, "Keys", SaveKeys);
        SetFunction(L, "Clear", SaveClear);
        SetFunction(L, "Flush", SaveFlush);
        lua_setglobal(L, "save");
    }
}
