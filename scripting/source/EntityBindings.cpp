// Entity (a userdata holding the id) and Component (an entity's component by name, its fields read
// and written through the function that saves it in scene files).
#include "Bindings.hpp"

#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Importers/SceneFile.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace ScriptingInternal
{
    namespace
    {
        using Serializer = SceneFile::detail::Serializer;

        struct ComponentRef
        {
            uint64_t entity;
            const Serializer* serializer;
        };

        Scene& SceneOf(lua_State* L) { return *GetContext(L).scene; }

        const Serializer& CheckSerializer(lua_State* L, int index)
        {
            const char* name = luaL_checkstring(L, index);
            const Serializer* serializer = SceneFile::FindComponent(name);
            if (!serializer)
                Error(L, "there is no component named '%s'", name);
            return *serializer;
        }

        // ---- Field values -------------------------------------------------------------------

        using Value = FieldVisitor::Value;

        void PushArray(lua_State* L, const float* values, int count)
        {
            lua_createtable(L, count, 0);
            for (int i = 0; i < count; ++i)
            {
                lua_pushnumber(L, values[i]);
                lua_rawseti(L, -2, i + 1);
            }
        }

        // Two or three floats are a vector, others a table.
        void PushFloats(lua_State* L, const float* values, int count)
        {
            if (count == 2 || count == 3)
                lua_pushvector(L, values[0], values[1], count == 3 ? values[2] : 0.0f);
            else
                PushArray(L, values, count);
        }

        void PushField(lua_State* L, const Value& value)
        {
            switch (value.type)
            {
            case Value::Type::Float: lua_pushnumber(L, *static_cast<const float*>(value.data)); break;
            case Value::Type::Int: lua_pushnumber(L, *static_cast<const int*>(value.data)); break;
            case Value::Type::UInt: lua_pushnumber(L, *static_cast<const uint32_t*>(value.data)); break;
            case Value::Type::Bool: lua_pushboolean(L, *static_cast<const bool*>(value.data)); break;
            case Value::Type::String: lua_pushstring(L, static_cast<const std::string*>(value.data)->c_str()); break;
            case Value::Type::Floats: PushFloats(L, static_cast<const float*>(value.data), value.count); break;
            case Value::Type::Quat: PushQuat(L, *static_cast<const glm::quat*>(value.data)); break;
            case Value::Type::FloatList:
            {
                const auto& list = *static_cast<const std::vector<float>*>(value.data);
                PushArray(L, list.data(), static_cast<int>(list.size()));
                break;
            }
            case Value::Type::StringList:
            {
                const auto& list = *static_cast<const std::vector<std::string>*>(value.data);
                lua_createtable(L, static_cast<int>(list.size()), 0);
                for (size_t i = 0; i < list.size(); ++i)
                {
                    lua_pushstring(L, list[i].c_str());
                    lua_rawseti(L, -2, static_cast<int>(i) + 1);
                }
                break;
            }
            }
        }

        [[noreturn]] void Expected(lua_State* L, const char* field, const char* what)
        {
            Error(L, "%s expects %s, got %s", field, what, luaL_typename(L, -1));
        }

        float Number(lua_State* L, int index, const char* field)
        {
            if (lua_type(L, index) != LUA_TNUMBER)
            {
                lua_pushvalue(L, index);
                Expected(L, field, "a number");
            }
            return static_cast<float>(lua_tonumber(L, index));
        }

        // The Luau value at `index` into the field (an error when it doesn't fit).
        void ReadField(lua_State* L, int index, const char* field, const Value& value)
        {
            index = lua_absindex(L, index);
            switch (value.type)
            {
            case Value::Type::Float: *static_cast<float*>(value.data) = Number(L, index, field); break;
            case Value::Type::Int: *static_cast<int*>(value.data) = static_cast<int>(Number(L, index, field)); break;
            case Value::Type::UInt:
            {
                const float n = Number(L, index, field);
                if (n < 0.0f)
                    Error(L, "%s can't be negative", field);
                *static_cast<uint32_t*>(value.data) = static_cast<uint32_t>(n);
                break;
            }
            case Value::Type::Bool:
                if (!lua_isboolean(L, index))
                {
                    lua_pushvalue(L, index);
                    Expected(L, field, "a boolean");
                }
                *static_cast<bool*>(value.data) = lua_toboolean(L, index) != 0;
                break;
            case Value::Type::String:
                if (lua_type(L, index) != LUA_TSTRING)
                {
                    lua_pushvalue(L, index);
                    Expected(L, field, "a string");
                }
                *static_cast<std::string*>(value.data) = lua_tostring(L, index);
                break;
            case Value::Type::Floats:
            {
                auto* out = static_cast<float*>(value.data);
                if ((value.count == 2 || value.count == 3) && lua_isvector(L, index))
                {
                    const float* v = lua_tovector(L, index);
                    for (int i = 0; i < value.count; ++i)
                        out[i] = v[i];
                }
                else if (value.count == 4 && IsQuat(L, index))
                {
                    const glm::quat q = CheckQuat(L, index);
                    out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w;
                }
                else if (lua_istable(L, index) && lua_objlen(L, index) == value.count)
                {
                    for (int i = 0; i < value.count; ++i)
                    {
                        lua_rawgeti(L, index, i + 1);
                        out[i] = Number(L, -1, field);
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    lua_pushvalue(L, index);
                    char what[48];
                    std::snprintf(what, sizeof(what), value.count <= 3 ? "a vector" : "a table of %d numbers", value.count);
                    Expected(L, field, what);
                }
                break;
            }
            case Value::Type::Quat: *static_cast<glm::quat*>(value.data) = CheckQuat(L, index); break;
            case Value::Type::FloatList:
            case Value::Type::StringList:
            {
                if (!lua_istable(L, index))
                {
                    lua_pushvalue(L, index);
                    Expected(L, field, "a table");
                }
                const int n = lua_objlen(L, index);
                if (value.type == Value::Type::FloatList)
                {
                    auto& list = *static_cast<std::vector<float>*>(value.data);
                    list.clear();
                    for (int i = 1; i <= n; ++i)
                    {
                        lua_rawgeti(L, index, i);
                        list.push_back(Number(L, -1, field));
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    auto& list = *static_cast<std::vector<std::string>*>(value.data);
                    list.clear();
                    for (int i = 1; i <= n; ++i)
                    {
                        lua_rawgeti(L, index, i);
                        const char* s = lua_tostring(L, -1);
                        list.push_back(s ? s : "");
                        lua_pop(L, 1);
                    }
                }
                break;
            }
            }
        }

        // Pushes the field named `wanted`.
        class FieldReader final : public FieldVisitor
        {
        public:
            FieldReader(lua_State* L, const char* wanted) : m_L(L), m_Wanted(wanted) {}
            void Visit(const char* name, const Value& value) override
            {
                if (found || std::strcmp(name, m_Wanted) != 0)
                    return;
                found = true;
                PushField(m_L, value);
            }
            bool found = false;

        private:
            lua_State* m_L;
            const char* m_Wanted;
        };

        // Writes the value at `index` into the field named `wanted`.
        class FieldWriter final : public FieldVisitor
        {
        public:
            FieldWriter(lua_State* L, int index, const char* wanted) : m_L(L), m_Index(index), m_Wanted(wanted) {}
            void Visit(const char* name, const Value& value) override
            {
                if (found || std::strcmp(name, m_Wanted) != 0)
                    return;
                found = true;
                ReadField(m_L, m_Index, name, value);
            }
            bool IsNumber(const char* name) const override
            {
                return std::strcmp(name, m_Wanted) == 0 && lua_type(m_L, m_Index) == LUA_TNUMBER;
            }
            bool found = false;

        private:
            lua_State* m_L;
            int m_Index;
            const char* m_Wanted;
        };

        // Writes every field the table at `index` names.
        class TableWriter final : public FieldVisitor
        {
        public:
            TableWriter(lua_State* L, int index) : m_L(L), m_Index(lua_absindex(L, index)) {}
            void Visit(const char* name, const Value& value) override
            {
                lua_getfield(m_L, m_Index, name);
                if (!lua_isnil(m_L, -1))
                {
                    ReadField(m_L, -1, name, value);
                    ++used;
                }
                lua_pop(m_L, 1);
            }
            bool IsNumber(const char* name) const override
            {
                lua_getfield(m_L, m_Index, name);
                const bool number = lua_type(m_L, -1) == LUA_TNUMBER;
                lua_pop(m_L, 1);
                return number;
            }
            int used = 0;

        private:
            lua_State* m_L;
            int m_Index;
        };

        class NoFields final : public FieldVisitor
        {
        public:
            void Visit(const char*, const Value&) override {}
        };

        int TableSize(lua_State* L, int index)
        {
            int n = 0;
            index = lua_absindex(L, index);
            lua_pushnil(L);
            while (lua_next(L, index) != 0)
            {
                ++n;
                lua_pop(L, 1);
            }
            return n;
        }

        // Adds or sets the component from the table at `index` (or with its defaults).
        void WriteComponent(lua_State* L, Entity entity, const Serializer& serializer, int index)
        {
            AssetManager2& assets = *GetContext(L).assets;
            if (lua_istable(L, index))
            {
                TableWriter writer(L, index);
                SceneArchive archive(writer, true, assets);
                serializer.load(entity, archive);
                if (writer.used != TableSize(L, index))
                    Error(L, "some fields given for %s aren't among its fields", serializer.name.c_str());
                return;
            }
            NoFields none;
            SceneArchive archive(none, true, assets);
            serializer.load(entity, archive);
        }

        // ---- Component ----------------------------------------------------------------------

        void PushComponent(lua_State* L, Entity entity, const Serializer& serializer)
        {
            auto* ref = static_cast<ComponentRef*>(lua_newuserdatataggedwithmetatable(L, sizeof(ComponentRef), kComponentTag));
            ref->entity = entity.RawId();
            ref->serializer = &serializer;
        }

        // The component's entity, while it still has the component.
        Entity CheckComponent(lua_State* L, int index, const Serializer** serializer)
        {
            const auto* ref = static_cast<const ComponentRef*>(lua_touserdatatagged(L, index, kComponentTag));
            if (!ref)
                luaL_typeerror(L, index, "Component");
            const Entity entity = SceneOf(L).FromId(ref->entity);
            if (!entity.IsValid() || !ref->serializer->has(entity))
                Error(L, "the entity no longer has its %s", ref->serializer->name.c_str());
            *serializer = ref->serializer;
            return entity;
        }

        Animator& CheckAnimator(lua_State* L)
        {
            const Serializer* serializer = nullptr;
            Entity entity = CheckComponent(L, 1, &serializer);
            Animator* animator = serializer->name == "Animator" ? entity.Get<Animator>() : nullptr;
            if (!animator)
                Error(L, "not an Animator");
            return *animator;
        }

        int ClipOf(lua_State* L, Animator& animator, int index)
        {
            if (lua_type(L, index) == LUA_TNUMBER)
                return lua_tointeger(L, index);
            const char* name = luaL_checkstring(L, index);
            const int clip = animator.FindClip(name);
            if (clip < 0)
                Error(L, "the Animator has no clip '%s'", name);
            return clip;
        }

        int AnimSetFloat(lua_State* L) { CheckAnimator(L).SetFloat(luaL_checkstring(L, 2), static_cast<float>(luaL_checknumber(L, 3))); return 0; }
        int AnimSetBool(lua_State* L) { CheckAnimator(L).SetBool(luaL_checkstring(L, 2), luaL_checkboolean(L, 3) != 0); return 0; }
        int AnimSetTrigger(lua_State* L) { CheckAnimator(L).SetTrigger(luaL_checkstring(L, 2)); return 0; }
        int AnimResetTrigger(lua_State* L) { CheckAnimator(L).ResetTrigger(luaL_checkstring(L, 2)); return 0; }
        int AnimGetFloat(lua_State* L) { lua_pushnumber(L, CheckAnimator(L).GetFloat(luaL_checkstring(L, 2))); return 1; }
        int AnimGetBool(lua_State* L) { lua_pushboolean(L, CheckAnimator(L).GetBool(luaL_checkstring(L, 2))); return 1; }
        int AnimPlay(lua_State* L)
        {
            Animator& animator = CheckAnimator(L);
            animator.Play(ClipOf(L, animator, 2), luaL_optboolean(L, 3, true) != 0);
            return 0;
        }
        int AnimCrossFade(lua_State* L)
        {
            Animator& animator = CheckAnimator(L);
            animator.CrossFade(ClipOf(L, animator, 2), static_cast<float>(luaL_optnumber(L, 3, 0.25)), luaL_optboolean(L, 4, true) != 0);
            return 0;
        }
        int AnimCrossFadeState(lua_State* L)
        {
            lua_pushboolean(L, CheckAnimator(L).CrossFadeState(luaL_checkstring(L, 2), static_cast<float>(luaL_optnumber(L, 3, 0.25))));
            return 1;
        }

        lua_CFunction AnimatorMethod(int atom)
        {
            switch (atom)
            {
            case kSetFloat: return AnimSetFloat;
            case kSetBool: return AnimSetBool;
            case kSetTrigger: return AnimSetTrigger;
            case kResetTrigger: return AnimResetTrigger;
            case kGetFloat: return AnimGetFloat;
            case kGetBool: return AnimGetBool;
            case kPlay: return AnimPlay;
            case kCrossFade: return AnimCrossFade;
            case kCrossFadeState: return AnimCrossFadeState;
            default: return nullptr;
            }
        }

        int ComponentIndex(lua_State* L)
        {
            const Serializer* serializer = nullptr;
            const Entity entity = CheckComponent(L, 1, &serializer);
            const char* name = nullptr;
            const int atom = KeyAtom(L, 2, &name);
            if (serializer->name == "Animator")
                if (lua_CFunction method = AnimatorMethod(atom))
                {
                    lua_pushcfunction(L, method, name);
                    return 1;
                }
            FieldReader reader(L, name);
            SceneArchive archive(reader, false, *GetContext(L).assets);
            serializer->save(entity, archive);
            if (!reader.found)
                Error(L, "%s has no field '%s'", serializer->name.c_str(), name);
            return 1;
        }

        int ComponentNewIndex(lua_State* L)
        {
            const Serializer* serializer = nullptr;
            const Entity entity = CheckComponent(L, 1, &serializer);
            const char* name = luaL_checkstring(L, 2);
            FieldWriter writer(L, 3, name);
            SceneArchive archive(writer, true, *GetContext(L).assets);
            serializer->load(entity, archive);
            if (!writer.found)
                Error(L, "%s has no field '%s'", serializer->name.c_str(), name);
            return 0;
        }

        int ComponentNamecall(lua_State* L)
        {
            int atom = -1;
            const char* name = lua_namecallatom(L, &atom);
            const Serializer* serializer = nullptr;
            CheckComponent(L, 1, &serializer);
            if (serializer->name == "Animator")
                if (lua_CFunction method = AnimatorMethod(atom))
                    return method(L);
            Error(L, "%s has no method '%s'", serializer->name.c_str(), name ? name : "?");
        }

        int ComponentToString(lua_State* L)
        {
            const auto* ref = static_cast<const ComponentRef*>(lua_touserdatatagged(L, 1, kComponentTag));
            const Entity entity = ref ? SceneOf(L).FromId(ref->entity) : Entity{};
            const std::string text = std::string(ref ? ref->serializer->name : "?") + " of " + (entity.IsValid() ? entity.GetName() : "(destroyed)");
            lua_pushstring(L, text.c_str());
            return 1;
        }

        // ---- Entity -------------------------------------------------------------------------

        int IsValid(lua_State* L)
        {
            lua_pushboolean(L, ToEntity(L, 1).IsValid());
            return 1;
        }

        int Destroy(lua_State* L)
        {
            const Entity entity = ToEntity(L, 1);
            if (entity.IsValid())
                GetContext(L).destroys.push_back(entity);
            return 0;
        }

        int Has(lua_State* L)
        {
            const Entity entity = CheckEntity(L, 1);
            lua_pushboolean(L, CheckSerializer(L, 2).has(entity));
            return 1;
        }

        int Get(lua_State* L)
        {
            const Entity entity = CheckEntity(L, 1);
            const Serializer& serializer = CheckSerializer(L, 2);
            if (serializer.has(entity))
                PushComponent(L, entity, serializer);
            else
                lua_pushnil(L);
            return 1;
        }

        // entity:Add(name, fields): adds the component (or sets the fields given if it has it).
        int Add(lua_State* L)
        {
            const Entity entity = CheckEntity(L, 1);
            const Serializer& serializer = CheckSerializer(L, 2);
            if (!serializer.has(entity) || lua_istable(L, 3))
                WriteComponent(L, entity, serializer, 3);
            PushComponent(L, entity, serializer);
            return 1;
        }

        int Remove(lua_State* L)
        {
            const Entity entity = CheckEntity(L, 1);
            const Serializer& serializer = CheckSerializer(L, 2);
            if (serializer.has(entity))
                serializer.remove(entity);
            return 0;
        }

        int GetScript(lua_State* L)
        {
            const Entity entity = CheckEntity(L, 1);
            const char* name = luaL_checkstring(L, 2);
            Context& context = GetContext(L);
            if (!context.pushScript || !context.pushScript(L, entity, name))
                lua_pushnil(L);
            return 1;
        }

        int GetChildren(lua_State* L)
        {
            Entity entity = CheckEntity(L, 1);
            lua_newtable(L);
            int i = 0;
            entity.ForEachChild([&](Entity child) {
                PushEntity(L, child);
                lua_rawseti(L, -2, ++i);
            });
            return 1;
        }

        Entity FindBelow(Entity entity, const char* name)
        {
            Entity found;
            entity.ForEachChild([&](Entity child) {
                if (found.IsValid())
                    return;
                if (std::strcmp(child.GetName(), name) == 0)
                    found = child;
                else
                    found = FindBelow(child, name);
            });
            return found;
        }

        // entity:FindChild(name): the first entity below it with that name (depth first).
        int FindChild(lua_State* L)
        {
            PushEntity(L, FindBelow(CheckEntity(L, 1), luaL_checkstring(L, 2)));
            return 1;
        }

        glm::quat LocalRotation(const LocalTRS& t)
        {
            return glm::quat(t.rot_quat[3], t.rot_quat[0], t.rot_quat[1], t.rot_quat[2]);
        }

        void SetLocalRotation(Entity entity, const glm::quat& q)
        {
            entity.SetRotationQuat(q.x, q.y, q.z, q.w);
        }

        // entity:Rotate(axis, degrees): turns it about its own axis.
        int Rotate(lua_State* L)
        {
            Entity entity = CheckEntity(L, 1);
            const glm::vec3 axis = CheckVector(L, 2);
            const float degrees = static_cast<float>(luaL_checknumber(L, 3));
            const LocalTRS* t = entity.TryGet<LocalTRS>();
            const float length = glm::length(axis);
            if (t && length > 0.0f)
                SetLocalRotation(entity, glm::normalize(LocalRotation(*t) * glm::angleAxis(glm::radians(degrees), axis / length)));
            return 0;
        }

        // entity:Translate(v): moves it along its own axes.
        int Translate(lua_State* L)
        {
            Entity entity = CheckEntity(L, 1);
            const glm::vec3 v = CheckVector(L, 2);
            if (const LocalTRS* t = entity.TryGet<LocalTRS>())
            {
                const glm::vec3 p = glm::vec3(t->pos[0], t->pos[1], t->pos[2]) + LocalRotation(*t) * v;
                entity.SetPosition(p.x, p.y, p.z);
            }
            return 0;
        }

        void SetWorldRotation(Scene& scene, Entity entity, const glm::quat& world)
        {
            const Entity parent = scene.Parent(entity);
            SetLocalRotation(entity, glm::normalize(parent.IsValid() ? glm::inverse(WorldRotation(scene, parent)) * world : world));
        }

        // entity:LookAt(target, up): turns its +Z toward a point or an entity.
        int LookAt(lua_State* L)
        {
            const Entity entity = CheckEntity(L, 1);
            Scene& scene = SceneOf(L);
            const glm::vec3 target = lua_isvector(L, 2) ? CheckVector(L, 2) : glm::vec3(WorldMatrix(scene, CheckEntity(L, 2))[3]);
            const glm::vec3 up = OptVector(L, 3, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::vec3 direction = target - glm::vec3(WorldMatrix(scene, entity)[3]);
            if (glm::length(direction) < 1e-6f)
                return 0;
            const glm::vec3 f = glm::normalize(direction);
            const glm::vec3 u = std::abs(glm::dot(f, glm::normalize(up))) > 0.9999f ? glm::vec3(0.0f, 0.0f, 1.0f) : up;
            SetWorldRotation(scene, entity, glm::quatLookAtLH(f, u));
            return 0;
        }

        lua_CFunction EntityMethod(int atom)
        {
            switch (atom)
            {
            case kIsValid: return IsValid;
            case kDestroy: return Destroy;
            case kHas: return Has;
            case kGet: return Get;
            case kAdd: return Add;
            case kRemove: return Remove;
            case kGetScript: return GetScript;
            case kGetChildren: return GetChildren;
            case kFindChild: return FindChild;
            case kRotate: return Rotate;
            case kTranslate: return Translate;
            case kLookAt: return LookAt;
            default: return nullptr;
            }
        }

        int EntityIndex(lua_State* L)
        {
            const char* name = nullptr;
            const int atom = KeyAtom(L, 2, &name);
            if (lua_CFunction method = EntityMethod(atom))
            {
                lua_pushcfunction(L, method, name);
                return 1;
            }
            const Entity entity = CheckEntity(L, 1);
            Scene& scene = SceneOf(L);
            const LocalTRS* t = entity.TryGet<LocalTRS>();
            const LocalTRS identity{};
            if (!t)
                t = &identity;
            switch (atom)
            {
            case kId: lua_pushnumber(L, static_cast<double>(entity.RawId())); return 1;
            case kName: lua_pushstring(L, entity.GetName()); return 1;
            case kParent: PushEntity(L, scene.Parent(entity)); return 1;
            case kPosition: lua_pushvector(L, t->pos[0], t->pos[1], t->pos[2]); return 1;
            case kRotation: PushQuat(L, LocalRotation(*t)); return 1;
            case kScale: lua_pushvector(L, t->scl[0], t->scl[1], t->scl[2]); return 1;
            case kWorldPosition: PushVector(L, glm::vec3(WorldMatrix(scene, entity)[3])); return 1;
            case kWorldRotation: PushQuat(L, WorldRotation(scene, entity)); return 1;
            case kForward: PushVector(L, WorldRotation(scene, entity) * glm::vec3(0.0f, 0.0f, 1.0f)); return 1;
            case kRight: PushVector(L, WorldRotation(scene, entity) * glm::vec3(1.0f, 0.0f, 0.0f)); return 1;
            case kUp: PushVector(L, WorldRotation(scene, entity) * glm::vec3(0.0f, 1.0f, 0.0f)); return 1;
            default: Error(L, "Entity has no member '%s'", name);
            }
        }

        int EntityNewIndex(lua_State* L)
        {
            Entity entity = CheckEntity(L, 1);
            Scene& scene = SceneOf(L);
            const char* name = nullptr;
            switch (KeyAtom(L, 2, &name))
            {
            case kName: entity.SetName(luaL_checkstring(L, 3)); return 0;
            case kParent:
                if (lua_isnil(L, 3))
                    entity.SetParent(Entity{});
                else
                {
                    const Entity parent = CheckEntity(L, 3);
                    for (Entity e = parent; e.IsValid(); e = scene.Parent(e))
                        if (e.RawId() == entity.RawId())
                            Error(L, "an entity can't go below itself");
                    entity.SetParent(parent);
                }
                return 0;
            case kPosition:
            {
                const glm::vec3 p = CheckVector(L, 3);
                entity.SetPosition(p.x, p.y, p.z);
                return 0;
            }
            case kRotation: SetLocalRotation(entity, CheckQuat(L, 3)); return 0;
            case kScale:
            {
                const glm::vec3 s = lua_type(L, 3) == LUA_TNUMBER ? glm::vec3(static_cast<float>(lua_tonumber(L, 3))) : CheckVector(L, 3);
                entity.SetScale(s.x, s.y, s.z);
                return 0;
            }
            case kWorldPosition:
            {
                glm::vec3 p = CheckVector(L, 3);
                if (const Entity parent = scene.Parent(entity); parent.IsValid())
                    p = glm::vec3(glm::inverse(WorldMatrix(scene, parent)) * glm::vec4(p, 1.0f));
                entity.SetPosition(p.x, p.y, p.z);
                return 0;
            }
            case kWorldRotation: SetWorldRotation(scene, entity, CheckQuat(L, 3)); return 0;
            default: Error(L, "can't set '%s' on an Entity", name);
            }
        }

        int EntityNamecall(lua_State* L)
        {
            int atom = -1;
            const char* name = lua_namecallatom(L, &atom);
            if (lua_CFunction method = EntityMethod(atom))
                return method(L);
            Error(L, "Entity has no method '%s'", name ? name : "?");
        }

        int EntityEq(lua_State* L)
        {
            const auto* a = static_cast<const uint64_t*>(lua_touserdatatagged(L, 1, kEntityTag));
            const auto* b = static_cast<const uint64_t*>(lua_touserdatatagged(L, 2, kEntityTag));
            lua_pushboolean(L, a && b && *a == *b);
            return 1;
        }

        int EntityToString(lua_State* L)
        {
            const auto* id = static_cast<const uint64_t*>(lua_touserdatatagged(L, 1, kEntityTag));
            const Entity entity = id ? SceneOf(L).FromId(*id) : Entity{};
            char text[160];
            if (entity.IsValid())
                std::snprintf(text, sizeof(text), "Entity(%llu '%s')", static_cast<unsigned long long>(entity.RawId()), entity.GetName());
            else
                std::snprintf(text, sizeof(text), "Entity(destroyed)");
            lua_pushstring(L, text);
            return 1;
        }

        void SetFunction(lua_State* L, const char* name, lua_CFunction fn)
        {
            lua_pushcfunction(L, fn, name);
            lua_setfield(L, -2, name);
        }

        void Metatable(lua_State* L, int tag, const char* type, lua_CFunction index, lua_CFunction newIndex, lua_CFunction namecall,
                       lua_CFunction toString, lua_CFunction eq)
        {
            lua_newtable(L);
            SetFunction(L, "__index", index);
            SetFunction(L, "__newindex", newIndex);
            SetFunction(L, "__namecall", namecall);
            SetFunction(L, "__tostring", toString);
            if (eq)
                SetFunction(L, "__eq", eq);
            lua_pushstring(L, type);
            lua_setfield(L, -2, "__type");
            lua_pushstring(L, "locked");
            lua_setfield(L, -2, "__metatable");
            lua_setuserdatametatable(L, tag);
        }
    }

    void OpenEntity(lua_State* L)
    {
        Metatable(L, kEntityTag, "Entity", EntityIndex, EntityNewIndex, EntityNamecall, EntityToString, EntityEq);
        Metatable(L, kComponentTag, "Component", ComponentIndex, ComponentNewIndex, ComponentNamecall, ComponentToString, nullptr);
    }
}
