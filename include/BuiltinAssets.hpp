#pragma once
#include <AssetsSystem/AssetManager.hpp>
#include <Texture.hpp>

class MaterialAsset;
class IMesh;

// The shapes PrimitiveMesh makes (Geometry/Primitives).
enum class BuiltinMesh { Cube, Sphere, Plane };

// Small engine-provided assets, created on first use and registered as persistent.
namespace BuiltinAssets
{
    // 1x1 white, in an sRGB (color data) or UNORM (non-color data) format.
    AssetHandle<Texture> WhiteTexture(AssetManager2& assets, bool srgb);

    // 1x1 tangent-space normal pointing straight out of the surface.
    AssetHandle<Texture> FlatNormalTexture(AssetManager2& assets);

    // Plain white, rough, non-metallic material for primitives that don't specify one.
    AssetHandle<MaterialAsset> DefaultMaterial(AssetManager2& assets);

    // A 1 m cube, a 1 m wide sphere or a 10 m ground plane, shared by everything that uses it.
    AssetHandle<IMesh> PrimitiveMesh(AssetManager2& assets, BuiltinMesh shape);
}
