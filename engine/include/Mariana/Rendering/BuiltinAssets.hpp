#pragma once
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <glm/glm.hpp>

class MaterialAsset;
class IMesh;

// The shapes PrimitiveMesh makes (Geometry/Primitives). Grid (a 1 m square in XZ facing +Y, 24 x 24
// cells) and SubdividedCube (a 1 m cube, 8 x 8 cells a face) are for soft bodies.
enum class BuiltinMesh { Cube, Sphere, Plane, Cylinder, Capsule, Grid, SubdividedCube };

// Small engine-provided assets, created on first use and registered as persistent.
namespace BuiltinAssets
{
    // 1x1 white, in an sRGB (color data) or UNORM (non-color data) format.
    AssetHandle<Texture> WhiteTexture(AssetManager2& assets, bool srgb);

    // 1x1 tangent-space normal pointing straight out of the surface.
    AssetHandle<Texture> FlatNormalTexture(AssetManager2& assets);

    // 256x256 tangent-space normal map of small waves that tiles (made in code), for water.
    AssetHandle<Texture> WaterNormalTexture(AssetManager2& assets);

    // Plain white, rough, non-metallic material for primitives that don't specify one.
    AssetHandle<MaterialAsset> DefaultMaterial(AssetManager2& assets);

    // The shape's mesh (a 1 m cube, a 1 m wide sphere, a 10 m ground plane, ...), shared by everything
    // that uses it and kept on the CPU too (MeshUsage::KeepData).
    AssetHandle<IMesh> PrimitiveMesh(AssetManager2& assets, BuiltinMesh shape);

    // The builtin registered under `key` ("builtin://mesh/Cube", "builtin://material/default", ...),
    // made if it doesn't exist yet; invalid for a key that names none.
    AssetID Resolve(AssetManager2& assets, const std::string& key);

    // A new entity drawing a primitive with a material of its own (so editing one leaves the
    // others alone) in `color`, casting shadows. Invalid when the mesh couldn't be made.
    Entity InstantiatePrimitive(Scene& scene, AssetManager2& assets, BuiltinMesh shape, const char* name, const glm::vec3& color = glm::vec3(1.0f));
}
