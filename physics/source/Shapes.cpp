#include "Shapes.hpp"

#include <Mariana/Foundation/Logger.hpp>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <cstring>
#include <functional>

namespace PhysicsInternal
{
    namespace
    {
        JPH::ShapeRefC Created(const JPH::ShapeSettings::ShapeResult& result)
        {
            if (result.HasError())
            {
                Logger::Warning(std::string("Physics: a shape couldn't be made: ") + result.GetError().c_str());
                return nullptr;
            }
            return result.Get();
        }

        JPH::RefConst<JPH::PhysicsMaterial> MaterialOf(const Collider& c)
        {
            return new ColliderMaterial(c.friction, c.restitution);
        }

        // Vertices at the same place become one, so seams (where UVs or normals split a vertex)
        // aren't open edges a rolling body bumps into.
        void Weld(const MeshCollisionData& mesh, JPH::VertexList& vertices, JPH::IndexedTriangleList& triangles)
        {
            struct PositionHash
            {
                size_t operator()(const glm::vec3& p) const
                {
                    uint32_t bits[3];
                    std::memcpy(bits, &p, sizeof(bits));
                    return (static_cast<size_t>(bits[0]) * 73856093u) ^ (static_cast<size_t>(bits[1]) * 19349663u) ^ (static_cast<size_t>(bits[2]) * 83492791u);
                }
            };
            std::unordered_map<glm::vec3, uint32_t, PositionHash> unique;
            unique.reserve(mesh.positions.size());
            std::vector<uint32_t> remap(mesh.positions.size());
            vertices.reserve(mesh.positions.size());
            for (size_t i = 0; i < mesh.positions.size(); ++i)
            {
                const glm::vec3& p = mesh.positions[i];
                auto [it, added] = unique.try_emplace(p, static_cast<uint32_t>(vertices.size()));
                if (added)
                    vertices.push_back(JPH::Float3(p.x, p.y, p.z));
                remap[i] = it->second;
            }
            triangles.reserve(mesh.triangles.size() / 3);
            for (size_t i = 0; i + 2 < mesh.triangles.size(); i += 3)
            {
                const uint32_t a = mesh.triangles[i], b = mesh.triangles[i + 1], c = mesh.triangles[i + 2];
                if (a >= remap.size() || b >= remap.size() || c >= remap.size())
                    continue;
                triangles.push_back(JPH::IndexedTriangle(remap[a], remap[b], remap[c], 0));
            }
        }

        JPH::ShapeRefC MakeMesh(const MeshCollisionData& mesh, const Collider& c)
        {
            JPH::VertexList vertices;
            JPH::IndexedTriangleList triangles;
            Weld(mesh, vertices, triangles);
            JPH::PhysicsMaterialList materials;
            materials.push_back(MaterialOf(c));
            JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles), std::move(materials));
            settings.SetEmbedded();
            return Created(settings.Create());
        }

        JPH::ShapeRefC MakeHull(const MeshCollisionData& mesh, const Collider& c)
        {
            JPH::Array<JPH::Vec3> points;
            points.reserve(mesh.positions.size());
            for (const glm::vec3& p : mesh.positions)
                points.push_back(ToJolt(p));
            JPH::ConvexHullShapeSettings settings(points, JPH::cDefaultConvexRadius, MaterialOf(c));
            settings.SetEmbedded();
            settings.SetDensity(std::max(c.density, 1e-3f));
            return Created(settings.Create());
        }

        JPH::ShapeRefC MakePrimitive(const ColliderGeometry& g, ShapeCache& cache)
        {
            JPH::RefConst<JPH::PhysicsMaterial> material = MaterialOf(*g.collider);
            const float density = std::max(g.collider->density, 1e-3f);
            switch (g.shape)
            {
            case ColliderShape::Box:
            {
                const JPH::Vec3 half = ToJolt(g.halfExtents);
                JPH::BoxShapeSettings settings(half, std::min(JPH::cDefaultConvexRadius, 0.5f * half.ReduceMin()), material);
                settings.SetEmbedded();
                settings.SetDensity(density);
                return Created(settings.Create());
            }
            case ColliderShape::Sphere:
            {
                JPH::SphereShapeSettings settings(g.radius, material);
                settings.SetEmbedded();
                settings.SetDensity(density);
                return Created(settings.Create());
            }
            case ColliderShape::Capsule:
            {
                if (g.halfHeight <= 1e-4f)
                {
                    JPH::SphereShapeSettings settings(g.radius, material);
                    settings.SetEmbedded();
                    settings.SetDensity(density);
                    return Created(settings.Create());
                }
                JPH::CapsuleShapeSettings settings(g.halfHeight, g.radius, material);
                settings.SetEmbedded();
                settings.SetDensity(density);
                return Created(settings.Create());
            }
            case ColliderShape::Cylinder:
            {
                JPH::CylinderShapeSettings settings(g.halfHeight, g.radius, std::min(JPH::cDefaultConvexRadius, 0.5f * std::min(g.halfHeight, g.radius)), material);
                settings.SetEmbedded();
                settings.SetDensity(density);
                return Created(settings.Create());
            }
            case ColliderShape::Mesh:
            case ColliderShape::ConvexHull:
            {
                JPH::ShapeRefC shape = cache.Get(g);
                if (!shape || g.scale == glm::vec3(1.0f))
                    return shape;
                JPH::ScaledShapeSettings settings(shape, ToJolt(g.scale));
                settings.SetEmbedded();
                return Created(settings.Create());
            }
            }
            return nullptr;
        }

        bool IsOffset(const ColliderGeometry& g)
        {
            return glm::length(g.position) > 1e-6f || std::abs(std::abs(g.rotation.w) - 1.0f) > 1e-6f;
        }
    }

    size_t ShapeCache::KeyHash::operator()(const Key& k) const
    {
        size_t h = std::hash<const void*>()(k.mesh);
        for (float v : { k.friction, k.restitution, k.density })
            h = h * 31 + std::hash<float>()(v);
        return h * 2 + (k.convex ? 1 : 0);
    }

    JPH::ShapeRefC ShapeCache::Get(const ColliderGeometry& g)
    {
        if (!g.mesh || !g.collider)
            return nullptr;
        const bool convex = g.shape == ColliderShape::ConvexHull;
        // Triangle meshes never move on their own, so their density doesn't matter.
        const Key key{ g.mesh.get(), convex, g.collider->friction, g.collider->restitution, convex ? g.collider->density : 0.0f };
        auto found = m_Shapes.find(key);
        if (found != m_Shapes.end() && found->second.mesh.lock() == g.mesh)
            return found->second.shape;

        JPH::ShapeRefC shape = convex ? MakeHull(*g.mesh, *g.collider) : MakeMesh(*g.mesh, *g.collider);
        if (shape)
            m_Shapes[key] = Entry{ g.mesh, shape };
        return shape;
    }

    void ShapeCache::Prune()
    {
        for (auto it = m_Shapes.begin(); it != m_Shapes.end();)
            it = it->second.mesh.expired() ? m_Shapes.erase(it) : std::next(it);
    }

    JPH::ShapeRefC MakeShape(const ColliderGeometry& part, ShapeCache& cache)
    {
        JPH::ShapeRefC shape = MakePrimitive(part, cache);
        if (!shape || !IsOffset(part))
            return shape;
        JPH::RotatedTranslatedShapeSettings settings(ToJolt(part.position), ToJolt(part.rotation), shape);
        settings.SetEmbedded();
        return Created(settings.Create());
    }

    JPH::ShapeRefC MakeShape(const std::vector<ColliderGeometry>& parts, ShapeCache& cache)
    {
        if (parts.empty())
            return nullptr;
        if (parts.size() == 1)
            return MakeShape(parts[0], cache);
        JPH::StaticCompoundShapeSettings compound;
        compound.SetEmbedded();
        int added = 0;
        for (const ColliderGeometry& part : parts)
        {
            if (JPH::ShapeRefC shape = MakePrimitive(part, cache))
            {
                compound.AddShape(ToJolt(part.position), ToJolt(part.rotation), shape);
                ++added;
            }
        }
        if (added == 0)
            return nullptr;
        return Created(compound.Create());
    }
}
