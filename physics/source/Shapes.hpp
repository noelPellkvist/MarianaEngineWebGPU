#pragma once
// Private to MarianaPhysics: Jolt shapes from collider geometry.
#include "JoltCommon.hpp"

#include <Mariana/Physics/ColliderGeometry.hpp>

#include <unordered_map>
#include <vector>

namespace PhysicsInternal
{
    // Triangle meshes and convex hulls are slow to make (a level's mesh can take a good part of a
    // second), so they are kept per mesh and surface, unscaled, for as long as their mesh lives:
    // Play after Play reuses them.
    class ShapeCache
    {
    public:
        JPH::ShapeRefC Get(const ColliderGeometry& g);
        // Forgets shapes of meshes that are gone.
        void Prune();

    private:
        struct Key
        {
            const MeshCollisionData* mesh;
            bool convex;
            float friction, restitution, density;
            bool operator==(const Key&) const = default;
        };
        struct KeyHash
        {
            size_t operator()(const Key& k) const;
        };
        struct Entry
        {
            std::weak_ptr<const MeshCollisionData> mesh;
            JPH::ShapeRefC shape;
        };
        std::unordered_map<Key, Entry, KeyHash> m_Shapes;
    };

    // One collider as is, one moved off the body's origin, or several as a compound (null when none
    // could be made). Meshes come from the cache and are scaled here.
    JPH::ShapeRefC MakeShape(const std::vector<ColliderGeometry>& parts, ShapeCache& cache);
    JPH::ShapeRefC MakeShape(const ColliderGeometry& part, ShapeCache& cache);
}
