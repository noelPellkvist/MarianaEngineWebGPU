// vec3 (Luau's own vector type, made by the global vec3) and quat (a userdata with the usual
// operations; angles in degrees).
#include "Bindings.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ScriptingInternal
{
    namespace
    {
        int Vec3(lua_State* L)
        {
            lua_pushvector(L, static_cast<float>(luaL_optnumber(L, 1, 0.0)), static_cast<float>(luaL_optnumber(L, 2, 0.0)),
                           static_cast<float>(luaL_optnumber(L, 3, 0.0)));
            return 1;
        }

        // Degrees as three numbers or a vector, starting at `index`.
        glm::vec3 Angles(lua_State* L, int index)
        {
            if (lua_isvector(L, index))
                return CheckVector(L, index);
            return { static_cast<float>(luaL_checknumber(L, index)), static_cast<float>(luaL_checknumber(L, index + 1)),
                     static_cast<float>(luaL_checknumber(L, index + 2)) };
        }

        int QuatInverse(lua_State* L)
        {
            PushQuat(L, glm::inverse(CheckQuat(L, 1)));
            return 1;
        }

        int QuatToEuler(lua_State* L)
        {
            PushVector(L, glm::degrees(glm::eulerAngles(CheckQuat(L, 1))));
            return 1;
        }

        int QuatIndex(lua_State* L)
        {
            const glm::quat q = CheckQuat(L, 1);
            const char* name = nullptr;
            switch (KeyAtom(L, 2, &name))
            {
            case kX: lua_pushnumber(L, q.x); return 1;
            case kY: lua_pushnumber(L, q.y); return 1;
            case kZ: lua_pushnumber(L, q.z); return 1;
            case kW: lua_pushnumber(L, q.w); return 1;
            case kInverse: lua_pushcfunction(L, QuatInverse, "Inverse"); return 1;
            case kToEuler: lua_pushcfunction(L, QuatToEuler, "ToEuler"); return 1;
            default: Error(L, "quat has no member '%s'", name);
            }
        }

        int QuatNamecall(lua_State* L)
        {
            int atom = -1;
            const char* name = lua_namecallatom(L, &atom);
            switch (atom)
            {
            case kInverse: return QuatInverse(L);
            case kToEuler: return QuatToEuler(L);
            default: Error(L, "quat has no method '%s'", name ? name : "?");
            }
        }

        // q * q, or q * v (v turned by q).
        int QuatMul(lua_State* L)
        {
            const glm::quat a = CheckQuat(L, 1);
            if (IsQuat(L, 2))
                PushQuat(L, glm::normalize(a * CheckQuat(L, 2)));
            else if (lua_isvector(L, 2))
                PushVector(L, a * CheckVector(L, 2));
            else
                luaL_typeerror(L, 2, "quat or vector");
            return 1;
        }

        int QuatEq(lua_State* L)
        {
            lua_pushboolean(L, CheckQuat(L, 1) == CheckQuat(L, 2));
            return 1;
        }

        int QuatToString(lua_State* L)
        {
            const glm::quat q = CheckQuat(L, 1);
            char text[96];
            std::snprintf(text, sizeof(text), "quat(%g, %g, %g, %g)", q.x, q.y, q.z, q.w);
            lua_pushstring(L, text);
            return 1;
        }

        // quat.new(x, y, z, w)
        int QuatNew(lua_State* L)
        {
            const glm::quat q(static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 1)),
                              static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)));
            PushQuat(L, glm::normalize(q));
            return 1;
        }

        // quat.euler(x, y, z) or quat.euler(v): degrees about X, Y and Z.
        int QuatEuler(lua_State* L)
        {
            PushQuat(L, glm::quat(glm::radians(Angles(L, 1))));
            return 1;
        }

        // quat.angleAxis(degrees, axis)
        int QuatAngleAxis(lua_State* L)
        {
            const float degrees = static_cast<float>(luaL_checknumber(L, 1));
            const glm::vec3 axis = CheckVector(L, 2);
            const float length = glm::length(axis);
            PushQuat(L, length > 0.0f ? glm::angleAxis(glm::radians(degrees), axis / length) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            return 1;
        }

        // quat.lookRotation(forward, up = vec3(0, 1, 0)): turns +Z to `forward` with +Y toward `up`.
        int QuatLookRotation(lua_State* L)
        {
            const glm::vec3 forward = CheckVector(L, 1);
            const glm::vec3 up = OptVector(L, 2, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(forward) < 1e-6f)
            {
                PushQuat(L, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                return 1;
            }
            const glm::vec3 f = glm::normalize(forward);
            // Looking straight along `up` would leave the roll undefined: use another up.
            const glm::vec3 u = std::abs(glm::dot(f, glm::normalize(up))) > 0.9999f ? glm::vec3(0.0f, 0.0f, 1.0f) : up;
            PushQuat(L, glm::normalize(glm::quatLookAtLH(f, u)));
            return 1;
        }

        // quat.fromToRotation(from, to): the shortest turn taking one direction to the other.
        int QuatFromTo(lua_State* L)
        {
            const glm::vec3 from = glm::normalize(CheckVector(L, 1));
            const glm::vec3 to = glm::normalize(CheckVector(L, 2));
            const float d = glm::dot(from, to);
            if (d < -0.99999f)
            {
                glm::vec3 axis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), from);
                if (glm::length(axis) < 1e-4f)
                    axis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), from);
                PushQuat(L, glm::angleAxis(glm::pi<float>(), glm::normalize(axis)));
                return 1;
            }
            const glm::vec3 c = glm::cross(from, to);
            PushQuat(L, glm::normalize(glm::quat(1.0f + d, c.x, c.y, c.z)));
            return 1;
        }

        int QuatSlerp(lua_State* L)
        {
            PushQuat(L, glm::slerp(CheckQuat(L, 1), CheckQuat(L, 2), static_cast<float>(luaL_checknumber(L, 3))));
            return 1;
        }

        int QuatAngle(lua_State* L)
        {
            const float d = std::abs(glm::dot(CheckQuat(L, 1), CheckQuat(L, 2)));
            lua_pushnumber(L, glm::degrees(2.0f * std::acos(std::min(d, 1.0f))));
            return 1;
        }

        void SetFunction(lua_State* L, const char* name, lua_CFunction fn)
        {
            lua_pushcfunction(L, fn, name);
            lua_setfield(L, -2, name);
        }
    }

    void PushQuat(lua_State* L, const glm::quat& q)
    {
        *static_cast<glm::quat*>(lua_newuserdatataggedwithmetatable(L, sizeof(glm::quat), kQuatTag)) = q;
    }

    bool IsQuat(lua_State* L, int index)
    {
        return lua_touserdatatagged(L, index, kQuatTag) != nullptr;
    }

    glm::quat CheckQuat(lua_State* L, int index)
    {
        const auto* q = static_cast<const glm::quat*>(lua_touserdatatagged(L, index, kQuatTag));
        if (!q)
            luaL_typeerror(L, index, "quat");
        return *q;
    }

    void OpenMath(lua_State* L)
    {
        lua_pushcfunction(L, Vec3, "vec3");
        lua_setglobal(L, "vec3");

        lua_newtable(L);
        SetFunction(L, "__index", QuatIndex);
        SetFunction(L, "__namecall", QuatNamecall);
        SetFunction(L, "__mul", QuatMul);
        SetFunction(L, "__eq", QuatEq);
        SetFunction(L, "__tostring", QuatToString);
        lua_pushstring(L, "quat");
        lua_setfield(L, -2, "__type");
        lua_pushstring(L, "locked");
        lua_setfield(L, -2, "__metatable");
        lua_setuserdatametatable(L, kQuatTag);

        lua_newtable(L);
        SetFunction(L, "new", QuatNew);
        SetFunction(L, "euler", QuatEuler);
        SetFunction(L, "angleAxis", QuatAngleAxis);
        SetFunction(L, "lookRotation", QuatLookRotation);
        SetFunction(L, "fromToRotation", QuatFromTo);
        SetFunction(L, "slerp", QuatSlerp);
        SetFunction(L, "angle", QuatAngle);
        PushQuat(L, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        lua_setfield(L, -2, "identity");
        lua_setglobal(L, "quat");
    }
}
