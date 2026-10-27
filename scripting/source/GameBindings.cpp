// The game's libraries: scene (finding, making and destroying entities), input, time and physics.
#include "Bindings.hpp"

#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/GameClock.hpp>
#include <Mariana/Importers/SceneFile.hpp>
#include <Mariana/Physics/PhysicsWorld.hpp>
#include <Mariana/Platform/IInput.hpp>
#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Scene/Prefab.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>

namespace ScriptingInternal
{
    namespace
    {
        void SetFunction(lua_State* L, const char* name, lua_CFunction fn)
        {
            lua_pushcfunction(L, fn, name);
            lua_setfield(L, -2, name);
        }

        // ---- scene ----------------------------------------------------------------------------

        int SceneFind(lua_State* L)
        {
            PushEntity(L, GetContext(L).scene->FindByName(luaL_checkstring(L, 1)));
            return 1;
        }

        void Place(lua_State* L, Entity entity, int positionIndex, int rotationIndex)
        {
            if (!lua_isnoneornil(L, positionIndex))
            {
                const glm::vec3 p = CheckVector(L, positionIndex);
                entity.SetPosition(p.x, p.y, p.z);
            }
            if (!lua_isnoneornil(L, rotationIndex))
            {
                const glm::quat q = CheckQuat(L, rotationIndex);
                entity.SetRotationQuat(q.x, q.y, q.z, q.w);
            }
        }

        // scene.Create(name, parent): an empty entity.
        int SceneCreate(lua_State* L)
        {
            Entity entity = GetContext(L).scene->Instantiate(luaL_optstring(L, 1, "Entity"));
            if (!lua_isnoneornil(L, 2))
                entity.SetParent(CheckEntity(L, 2));
            PushEntity(L, entity);
            return 1;
        }

        // scene.Spawn(path, position, rotation): an instance of a model (or any prefab file).
        int SceneSpawn(lua_State* L)
        {
            Context& context = GetContext(L);
            const char* path = luaL_checkstring(L, 1);
            const std::filesystem::path file = FileSystem::Resolve(path);
            if (file.empty())
                Error(L, "no file %s", path);
            AssetHandle<Prefab> prefab = context.assets->Load<Prefab>(file);
            if (!prefab.IsValid() || !prefab.Get())
                Error(L, "could not load %s", path);
            Entity root = context.scene->Instantiate(*prefab.Get());
            Place(L, root, 2, 3);
            PushEntity(L, root);
            return 1;
        }

        // scene.SpawnPrimitive(shape, position, color): a cube, sphere, plane, cylinder or capsule
        // with a material of its own.
        int SceneSpawnPrimitive(lua_State* L)
        {
            static const char* const shapes[] = { "cube", "sphere", "plane", "cylinder", "capsule", nullptr };
            static const BuiltinMesh meshes[] = { BuiltinMesh::Cube, BuiltinMesh::Sphere, BuiltinMesh::Plane, BuiltinMesh::Cylinder, BuiltinMesh::Capsule };
            static const char* const names[] = { "Cube", "Sphere", "Plane", "Cylinder", "Capsule" };
            const int shape = luaL_checkoption(L, 1, nullptr, shapes);
            Context& context = GetContext(L);
            Entity entity = BuiltinAssets::InstantiatePrimitive(*context.scene, *context.assets, meshes[shape], names[shape],
                                                               OptVector(L, 3, glm::vec3(1.0f)));
            Place(L, entity, 2, 4);
            PushEntity(L, entity);
            return 1;
        }

        // scene.Each(component, fn): fn(entity) for every entity with the component, in id order.
        int SceneEach(lua_State* L)
        {
            const char* name = luaL_checkstring(L, 1);
            luaL_checktype(L, 2, LUA_TFUNCTION);
            const SceneFile::detail::Serializer* serializer = SceneFile::FindComponent(name);
            if (!serializer)
                Error(L, "there is no component named '%s'", name);
            if (!serializer->each)
                Error(L, "%s is a tag; entities can't be listed by it", name);
            Scene& scene = *GetContext(L).scene;
            std::vector<uint64_t> ids;
            serializer->each(scene, [&](Entity e) { ids.push_back(e.RawId()); });
            std::sort(ids.begin(), ids.end());
            // The callbacks may change the scene: each entity is looked up again.
            for (uint64_t id : ids)
            {
                const Entity entity = scene.FromId(id);
                if (!entity.IsValid())
                    continue;
                lua_pushvalue(L, 2);
                PushEntity(L, entity);
                lua_call(L, 1, 0);
            }
            return 0;
        }

        int SceneDestroy(lua_State* L)
        {
            const Entity entity = ToEntity(L, 1);
            if (entity.IsValid())
                GetContext(L).destroys.push_back(entity);
            return 0;
        }

        // ---- input ----------------------------------------------------------------------------

        // A key by name (case doesn't matter); the second key, when there is one, counts too
        // ("shift" is either shift key).
        std::pair<Key, Key> KeyByName(lua_State* L, int index)
        {
            static const std::unordered_map<std::string, std::pair<Key, Key>> keys = [] {
                std::unordered_map<std::string, std::pair<Key, Key>> map;
                for (char c = 'a'; c <= 'z'; ++c)
                    map[std::string(1, c)] = { static_cast<Key>(static_cast<int>(Key::A) + (c - 'a')), Key::UNKNOWN };
                for (char c = '0'; c <= '9'; ++c)
                    map[std::string(1, c)] = { static_cast<Key>(static_cast<int>(Key::KEY_0) + (c - '0')), Key::UNKNOWN };
                for (int f = 1; f <= 12; ++f)
                    map["f" + std::to_string(f)] = { static_cast<Key>(static_cast<int>(Key::F1) + f - 1), Key::UNKNOWN };
                const std::pair<const char*, std::pair<Key, Key>> named[] = {
                    { "space", { Key::SPACE, Key::UNKNOWN } }, { "enter", { Key::ENTER, Key::KP_ENTER } },
                    { "return", { Key::ENTER, Key::KP_ENTER } }, { "escape", { Key::ESCAPE, Key::UNKNOWN } },
                    { "tab", { Key::TAB, Key::UNKNOWN } }, { "backspace", { Key::BACKSPACE, Key::UNKNOWN } },
                    { "insert", { Key::INSERT, Key::UNKNOWN } }, { "delete", { Key::DELETE, Key::UNKNOWN } },
                    { "left", { Key::LEFT, Key::UNKNOWN } }, { "right", { Key::RIGHT, Key::UNKNOWN } },
                    { "up", { Key::UP, Key::UNKNOWN } }, { "down", { Key::DOWN, Key::UNKNOWN } },
                    { "pageup", { Key::PAGE_UP, Key::UNKNOWN } }, { "pagedown", { Key::PAGE_DOWN, Key::UNKNOWN } },
                    { "home", { Key::HOME, Key::UNKNOWN } }, { "end", { Key::END, Key::UNKNOWN } },
                    { "shift", { Key::LEFT_SHIFT, Key::RIGHT_SHIFT } }, { "leftshift", { Key::LEFT_SHIFT, Key::UNKNOWN } },
                    { "rightshift", { Key::RIGHT_SHIFT, Key::UNKNOWN } }, { "control", { Key::LEFT_CONTROL, Key::RIGHT_CONTROL } },
                    { "ctrl", { Key::LEFT_CONTROL, Key::RIGHT_CONTROL } }, { "leftcontrol", { Key::LEFT_CONTROL, Key::UNKNOWN } },
                    { "rightcontrol", { Key::RIGHT_CONTROL, Key::UNKNOWN } }, { "alt", { Key::LEFT_ALT, Key::RIGHT_ALT } },
                    { "leftalt", { Key::LEFT_ALT, Key::UNKNOWN } }, { "rightalt", { Key::RIGHT_ALT, Key::UNKNOWN } },
                    { "minus", { Key::MINUS, Key::UNKNOWN } }, { "equal", { Key::EQUAL, Key::UNKNOWN } },
                    { "comma", { Key::COMMA, Key::UNKNOWN } }, { "period", { Key::PERIOD, Key::UNKNOWN } },
                    { "slash", { Key::SLASH, Key::UNKNOWN } }, { "semicolon", { Key::SEMICOLON, Key::UNKNOWN } },
                    { "apostrophe", { Key::APOSTROPHE, Key::UNKNOWN } }, { "grave", { Key::GRAVE_ACCENT, Key::UNKNOWN } },
                    { "leftbracket", { Key::LEFT_BRACKET, Key::UNKNOWN } }, { "rightbracket", { Key::RIGHT_BRACKET, Key::UNKNOWN } },
                    { "backslash", { Key::BACKSLASH, Key::UNKNOWN } },
                };
                for (const auto& [name, key] : named)
                    map[name] = key;
                return map;
            }();
            std::string name = luaL_checkstring(L, index);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const auto it = keys.find(name);
            if (it == keys.end())
                Error(L, "there is no key named '%s'", lua_tostring(L, index));
            return it->second;
        }

        template <bool (IInput::*Query)(Key) const>
        int KeyQuery(lua_State* L)
        {
            const auto [a, b] = KeyByName(L, 1);
            const IInput* input = GetContext(L).input;
            lua_pushboolean(L, input && ((input->*Query)(a) || (b != Key::UNKNOWN && (input->*Query)(b))));
            return 1;
        }

        template <bool (IInput::*Query)(MouseButton) const>
        int ButtonQuery(lua_State* L)
        {
            const int button = luaL_optinteger(L, 1, 0);
            luaL_argcheck(L, button >= 0 && button <= 4, 1, "a mouse button is 0 (left), 1 (right), 2 (middle), 3 or 4");
            const IInput* input = GetContext(L).input;
            lua_pushboolean(L, input && (input->*Query)(static_cast<MouseButton>(button)));
            return 1;
        }

        template <void (IInput::*Query)(double&, double&) const>
        int MouseQuery(lua_State* L)
        {
            double x = 0.0, y = 0.0;
            if (const IInput* input = GetContext(L).input)
                (input->*Query)(x, y);
            lua_pushvector(L, static_cast<float>(x), static_cast<float>(y), 0.0f);
            return 1;
        }

        // ---- time -----------------------------------------------------------------------------

        int TimeIndex(lua_State* L)
        {
            const GameClock& clock = *GetContext(L).clock;
            const char* name = nullptr;
            switch (KeyAtom(L, 2, &name))
            {
            case kDelta: lua_pushnumber(L, clock.DeltaTime()); return 1;
            case kTime: lua_pushnumber(L, clock.Time()); return 1;
            case kFrame: lua_pushnumber(L, static_cast<double>(clock.Frame())); return 1;
            case kScale: lua_pushnumber(L, clock.timeScale); return 1;
            default: Error(L, "time has no member '%s'", name);
            }
        }

        int TimeNewIndex(lua_State* L)
        {
            const char* name = nullptr;
            if (KeyAtom(L, 2, &name) != kScale)
                Error(L, "can't set time.%s", name);
            const double scale = luaL_checknumber(L, 3);
            luaL_argcheck(L, scale >= 0.0, 3, "the time scale can't be negative");
            GetContext(L).clock->timeScale = static_cast<float>(scale);
            return 0;
        }

        // ---- physics --------------------------------------------------------------------------

        PhysicsWorld& Physics(lua_State* L)
        {
            PhysicsWorld* physics = GetContext(L).physics;
            if (!physics)
                Error(L, "there is no physics here");
            return *physics;
        }

        // The options table at `index`: { ignore = entity, triggers = bool, layers = mask }.
        QueryFilter Filter(lua_State* L, int index)
        {
            QueryFilter filter;
            if (lua_isnoneornil(L, index))
                return filter;
            luaL_checktype(L, index, LUA_TTABLE);
            lua_getfield(L, index, "ignore");
            filter.ignore = ToEntity(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, index, "triggers");
            filter.triggers = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            lua_getfield(L, index, "layers");
            if (lua_type(L, -1) == LUA_TNUMBER)
                filter.layers = static_cast<uint32_t>(lua_tonumber(L, -1));
            lua_pop(L, 1);
            return filter;
        }

        void PushHit(lua_State* L, const RaycastHit& hit)
        {
            lua_createtable(L, 0, 4);
            PushEntity(L, hit.entity);
            lua_setfield(L, -2, "entity");
            PushVector(L, hit.point);
            lua_setfield(L, -2, "point");
            PushVector(L, hit.normal);
            lua_setfield(L, -2, "normal");
            lua_pushnumber(L, hit.distance);
            lua_setfield(L, -2, "distance");
        }

        void PushEntities(lua_State* L, const std::vector<Entity>& entities)
        {
            lua_createtable(L, static_cast<int>(entities.size()), 0);
            int i = 0;
            for (const Entity& e : entities)
            {
                PushEntity(L, e);
                lua_rawseti(L, -2, ++i);
            }
        }

        // physics.Raycast(origin, direction, maxDistance, options): the nearest hit or nil.
        int Raycast(lua_State* L)
        {
            RaycastHit hit;
            if (Physics(L).Raycast(CheckVector(L, 1), CheckVector(L, 2), static_cast<float>(luaL_optnumber(L, 3, 1000.0)), hit, Filter(L, 4)))
                PushHit(L, hit);
            else
                lua_pushnil(L);
            return 1;
        }

        int RaycastAll(lua_State* L)
        {
            const std::vector<RaycastHit> hits =
                Physics(L).RaycastAll(CheckVector(L, 1), CheckVector(L, 2), static_cast<float>(luaL_optnumber(L, 3, 1000.0)), Filter(L, 4));
            lua_createtable(L, static_cast<int>(hits.size()), 0);
            for (size_t i = 0; i < hits.size(); ++i)
            {
                PushHit(L, hits[i]);
                lua_rawseti(L, -2, static_cast<int>(i) + 1);
            }
            return 1;
        }

        // physics.SphereCast(origin, radius, direction, maxDistance, options)
        int SphereCast(lua_State* L)
        {
            RaycastHit hit;
            const QueryShape shape = QueryShape::Sphere(static_cast<float>(luaL_checknumber(L, 2)));
            if (Physics(L).ShapeCast(shape, CheckVector(L, 1), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), CheckVector(L, 3),
                                     static_cast<float>(luaL_optnumber(L, 4, 1000.0)), hit, Filter(L, 5)))
                PushHit(L, hit);
            else
                lua_pushnil(L);
            return 1;
        }

        // physics.OverlapSphere(center, radius, options): the entities whose bodies it touches.
        int OverlapSphere(lua_State* L)
        {
            PushEntities(L, Physics(L).Overlap(QueryShape::Sphere(static_cast<float>(luaL_checknumber(L, 2))), CheckVector(L, 1),
                                               glm::quat(1.0f, 0.0f, 0.0f, 0.0f), Filter(L, 3)));
            return 1;
        }

        // physics.OverlapBox(center, halfExtents, rotation, options)
        int OverlapBox(lua_State* L)
        {
            const glm::quat rotation = lua_isnoneornil(L, 3) ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f) : CheckQuat(L, 3);
            PushEntities(L, Physics(L).Overlap(QueryShape::Box(CheckVector(L, 2)), CheckVector(L, 1), rotation, Filter(L, 4)));
            return 1;
        }

        template <bool (PhysicsWorld::*Push)(Entity, const glm::vec3&)>
        int VectorOnBody(lua_State* L)
        {
            lua_pushboolean(L, (Physics(L).*Push)(CheckEntity(L, 1), CheckVector(L, 2)));
            return 1;
        }

        template <bool (PhysicsWorld::*Push)(Entity, const glm::vec3&, const glm::vec3&)>
        int VectorAtPoint(lua_State* L)
        {
            lua_pushboolean(L, (Physics(L).*Push)(CheckEntity(L, 1), CheckVector(L, 2), CheckVector(L, 3)));
            return 1;
        }

        // physics.SetVelocity(entity, linear, angular): angular stays as it is when not given.
        int SetVelocity(lua_State* L)
        {
            PhysicsWorld& physics = Physics(L);
            const Entity entity = CheckEntity(L, 1);
            glm::vec3 linear(0.0f), angular(0.0f);
            physics.GetVelocity(entity, linear, angular);
            lua_pushboolean(L, physics.SetVelocity(entity, CheckVector(L, 2), OptVector(L, 3, angular)));
            return 1;
        }

        // physics.GetVelocity(entity): linear, angular (nil when it isn't a dynamic body).
        int GetVelocity(lua_State* L)
        {
            glm::vec3 linear(0.0f), angular(0.0f);
            if (!Physics(L).GetVelocity(CheckEntity(L, 1), linear, angular))
            {
                lua_pushnil(L);
                return 1;
            }
            PushVector(L, linear);
            PushVector(L, angular);
            return 2;
        }

        // physics.Teleport(entity, position, rotation): rotation stays as it is when not given.
        int Teleport(lua_State* L)
        {
            const Entity entity = CheckEntity(L, 1);
            const glm::quat rotation = lua_isnoneornil(L, 3) ? WorldRotation(*GetContext(L).scene, entity) : CheckQuat(L, 3);
            lua_pushboolean(L, Physics(L).Teleport(entity, CheckVector(L, 2), rotation));
            return 1;
        }

        int WakeUp(lua_State* L)
        {
            lua_pushboolean(L, Physics(L).WakeUp(CheckEntity(L, 1)));
            return 1;
        }

        int IsTouching(lua_State* L)
        {
            lua_pushboolean(L, Physics(L).IsTouching(CheckEntity(L, 1), CheckEntity(L, 2)));
            return 1;
        }

        int GetBodyInfo(lua_State* L)
        {
            BodyInfo info;
            if (!Physics(L).GetBodyInfo(CheckEntity(L, 1), info))
            {
                lua_pushnil(L);
                return 1;
            }
            static const char* const motions[] = { "Static", "Kinematic", "Dynamic" };
            lua_createtable(L, 0, 6);
            lua_pushstring(L, motions[static_cast<int>(info.motion)]);
            lua_setfield(L, -2, "motion");
            lua_pushboolean(L, info.sleeping);
            lua_setfield(L, -2, "sleeping");
            lua_pushboolean(L, info.sensor);
            lua_setfield(L, -2, "sensor");
            lua_pushnumber(L, info.mass);
            lua_setfield(L, -2, "mass");
            PushVector(L, info.linearVelocity);
            lua_setfield(L, -2, "linearVelocity");
            PushVector(L, info.angularVelocity);
            lua_setfield(L, -2, "angularVelocity");
            return 1;
        }

        int GetVehicleInfo(lua_State* L)
        {
            VehicleInfo info;
            if (!Physics(L).GetVehicleInfo(CheckEntity(L, 1), info))
            {
                lua_pushnil(L);
                return 1;
            }
            lua_createtable(L, 0, 4);
            lua_pushnumber(L, info.speed);
            lua_setfield(L, -2, "speed");
            lua_pushnumber(L, info.rpm);
            lua_setfield(L, -2, "rpm");
            lua_pushnumber(L, info.gear);
            lua_setfield(L, -2, "gear");
            lua_createtable(L, static_cast<int>(info.wheels.size()), 0);
            for (size_t i = 0; i < info.wheels.size(); ++i)
            {
                const VehicleInfo::Wheel& w = info.wheels[i];
                lua_createtable(L, 0, 6);
                lua_pushboolean(L, w.contact);
                lua_setfield(L, -2, "contact");
                lua_pushnumber(L, w.suspension);
                lua_setfield(L, -2, "suspension");
                lua_pushnumber(L, w.steerAngle);
                lua_setfield(L, -2, "steerAngle");
                lua_pushnumber(L, w.rpm);
                lua_setfield(L, -2, "rpm");
                lua_pushnumber(L, w.longitudinalSlip);
                lua_setfield(L, -2, "longitudinalSlip");
                lua_pushnumber(L, w.lateralSlip);
                lua_setfield(L, -2, "lateralSlip");
                lua_rawseti(L, -2, static_cast<int>(i) + 1);
            }
            lua_setfield(L, -2, "wheels");
            return 1;
        }

        int GetCharacterInfo(lua_State* L)
        {
            CharacterInfo info;
            if (!Physics(L).GetCharacterInfo(CheckEntity(L, 1), info))
            {
                lua_pushnil(L);
                return 1;
            }
            lua_createtable(L, 0, 5);
            lua_pushboolean(L, info.grounded);
            lua_setfield(L, -2, "grounded");
            lua_pushboolean(L, info.onSteepGround);
            lua_setfield(L, -2, "onSteepGround");
            PushVector(L, info.velocity);
            lua_setfield(L, -2, "velocity");
            PushVector(L, info.groundNormal);
            lua_setfield(L, -2, "groundNormal");
            PushEntity(L, info.ground);
            lua_setfield(L, -2, "ground");
            return 1;
        }

        int SetRagdollMode(lua_State* L)
        {
            static const char* const modes[] = { "Animated", "Powered", "Limp", nullptr };
            Physics(L).SetRagdollMode(CheckEntity(L, 1), static_cast<RagdollMode>(luaL_checkoption(L, 2, nullptr, modes)));
            return 0;
        }

        // physics.WaterAt(point): the surface point and normal of the water it is in, or nil.
        int WaterAt(lua_State* L)
        {
            WaterSurface surface;
            if (!Physics(L).WaterAt(CheckVector(L, 1), surface))
            {
                lua_pushnil(L);
                return 1;
            }
            PushVector(L, surface.point);
            PushVector(L, surface.normal);
            return 2;
        }
    }

    void OpenGame(lua_State* L)
    {
        lua_newtable(L);
        SetFunction(L, "Find", SceneFind);
        SetFunction(L, "Create", SceneCreate);
        SetFunction(L, "Spawn", SceneSpawn);
        SetFunction(L, "SpawnPrimitive", SceneSpawnPrimitive);
        SetFunction(L, "Each", SceneEach);
        SetFunction(L, "Destroy", SceneDestroy);
        lua_setglobal(L, "scene");

        lua_newtable(L);
        SetFunction(L, "GetKey", KeyQuery<&IInput::IsKeyDown>);
        SetFunction(L, "GetKeyDown", KeyQuery<&IInput::IsKeyPressed>);
        SetFunction(L, "GetKeyUp", KeyQuery<&IInput::IsKeyReleased>);
        SetFunction(L, "GetMouseButton", ButtonQuery<&IInput::IsMouseButtonDown>);
        SetFunction(L, "GetMouseButtonDown", ButtonQuery<&IInput::IsMouseButtonPressed>);
        SetFunction(L, "GetMouseButtonUp", ButtonQuery<&IInput::IsMouseButtonReleased>);
        SetFunction(L, "GetMousePosition", MouseQuery<&IInput::GetMousePosition>);
        SetFunction(L, "GetMouseDelta", MouseQuery<&IInput::GetMouseDelta>);
        SetFunction(L, "GetScroll", MouseQuery<&IInput::GetMouseScrollDelta>);
        lua_setglobal(L, "input");

        lua_newtable(L);
        SetFunction(L, "__index", TimeIndex);
        SetFunction(L, "__newindex", TimeNewIndex);
        lua_pushstring(L, "Time");
        lua_setfield(L, -2, "__type");
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
        lua_setuserdatametatable(L, kTimeTag);
        lua_newuserdatataggedwithmetatable(L, 1, kTimeTag);
        lua_setglobal(L, "time");

        lua_newtable(L);
        SetFunction(L, "Raycast", Raycast);
        SetFunction(L, "RaycastAll", RaycastAll);
        SetFunction(L, "SphereCast", SphereCast);
        SetFunction(L, "OverlapSphere", OverlapSphere);
        SetFunction(L, "OverlapBox", OverlapBox);
        SetFunction(L, "AddForce", VectorOnBody<&PhysicsWorld::AddForce>);
        SetFunction(L, "AddForceAtPoint", VectorAtPoint<&PhysicsWorld::AddForceAtPoint>);
        SetFunction(L, "AddTorque", VectorOnBody<&PhysicsWorld::AddTorque>);
        SetFunction(L, "AddImpulse", VectorOnBody<&PhysicsWorld::AddImpulse>);
        SetFunction(L, "AddImpulseAtPoint", VectorAtPoint<&PhysicsWorld::AddImpulseAtPoint>);
        SetFunction(L, "AddAngularImpulse", VectorOnBody<&PhysicsWorld::AddAngularImpulse>);
        SetFunction(L, "AddRagdollImpulse", VectorOnBody<&PhysicsWorld::AddRagdollImpulse>);
        SetFunction(L, "SetVelocity", SetVelocity);
        SetFunction(L, "GetVelocity", GetVelocity);
        SetFunction(L, "Teleport", Teleport);
        SetFunction(L, "WakeUp", WakeUp);
        SetFunction(L, "IsTouching", IsTouching);
        SetFunction(L, "GetBodyInfo", GetBodyInfo);
        SetFunction(L, "GetVehicleInfo", GetVehicleInfo);
        SetFunction(L, "GetCharacterInfo", GetCharacterInfo);
        SetFunction(L, "SetRagdollMode", SetRagdollMode);
        SetFunction(L, "WaterAt", WaterAt);
        lua_setglobal(L, "physics");
    }
}
