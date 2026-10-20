#pragma once
// Private to MarianaPhysics: Jolt, and what the library puts around it.
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <Mariana/Physics/PhysicsSettings.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace PhysicsInternal
{
    inline JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    inline JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w).Normalized(); }
    inline glm::vec3 ToGlm(JPH::Vec3Arg v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
    inline glm::quat ToGlm(JPH::QuatArg q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

    // Object layers: the user layer (PhysicsSettings::layers) shifted up one, and whether the body
    // moves in the low bit, which picks its broad-phase layer.
    inline JPH::ObjectLayer MakeLayer(uint32_t layer, bool moving)
    {
        return static_cast<JPH::ObjectLayer>((std::min<uint32_t>(layer, PhysicsSettings::kMaxLayers - 1) << 1) | (moving ? 1u : 0u));
    }
    inline uint32_t UserLayer(JPH::ObjectLayer layer) { return static_cast<uint32_t>(layer) >> 1; }
    inline bool IsMoving(JPH::ObjectLayer layer) { return (layer & 1u) != 0; }

    namespace BroadPhase
    {
        constexpr JPH::BroadPhaseLayer Static(0);
        constexpr JPH::BroadPhaseLayer Moving(1);
    }

    class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
    {
    public:
        JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override { return IsMoving(layer) ? BroadPhase::Moving : BroadPhase::Static; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override { return layer == BroadPhase::Moving ? "Moving" : "Static"; }
#endif
    };

    // Static bodies are only tested against moving ones.
    class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override
        {
            return IsMoving(layer) || broadPhase == BroadPhase::Moving;
        }
    };

    // The layer matrix of PhysicsSettings.
    class ObjectPairs final : public JPH::ObjectLayerPairFilter
    {
    public:
        uint32_t collisions[PhysicsSettings::kMaxLayers];

        ObjectPairs() { std::fill(std::begin(collisions), std::end(collisions), ~0u); }

        void Set(const PhysicsSettings& settings)
        {
            for (int i = 0; i < PhysicsSettings::kMaxLayers; ++i)
                collisions[i] = i < static_cast<int>(settings.collisions.size()) ? settings.collisions[i] : ~0u;
        }

        bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
        {
            if (!IsMoving(a) && !IsMoving(b))
                return false;
            return (collisions[UserLayer(a)] >> UserLayer(b)) & 1u;
        }
    };

    // A collider's surface; contacts combine these per sub-shape, so a compound body's colliders keep
    // their own friction and bounciness.
    class ColliderMaterial final : public JPH::PhysicsMaterial
    {
    public:
        ColliderMaterial(float friction, float restitution) : friction(friction), restitution(restitution) {}
        float friction;
        float restitution;
    };

    inline float Friction(const JPH::Body& body, const JPH::SubShapeID& subShape)
    {
        if (const auto* material = dynamic_cast<const ColliderMaterial*>(body.GetShape()->GetMaterial(subShape)))
            return material->friction;
        return body.GetFriction();
    }

    inline float Restitution(const JPH::Body& body, const JPH::SubShapeID& subShape)
    {
        if (const auto* material = dynamic_cast<const ColliderMaterial*>(body.GetShape()->GetMaterial(subShape)))
            return material->restitution;
        return body.GetRestitution();
    }

    // Like most engines: the geometric mean of the frictions, the larger restitution.
    inline float CombineFriction(const JPH::Body& a, const JPH::SubShapeID& subA, const JPH::Body& b, const JPH::SubShapeID& subB)
    {
        return std::sqrt(std::max(Friction(a, subA), 0.0f) * std::max(Friction(b, subB), 0.0f));
    }

    inline float CombineRestitution(const JPH::Body& a, const JPH::SubShapeID& subA, const JPH::Body& b, const JPH::SubShapeID& subB)
    {
        return std::max(Restitution(a, subA), Restitution(b, subB));
    }
}
