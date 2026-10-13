#pragma once
#include <AssetsSystem/AssetManager.hpp>
#include <Texture.hpp>

class Shader2;
class MaterialAsset;

// Small engine-provided assets, created on first use and registered as persistent.
namespace BuiltinAssets
{
    // 1x1 white, in an sRGB (color data) or UNORM (non-color data) format.
    AssetHandle<Texture> WhiteTexture(AssetManager2& assets, bool srgb);

    // 1x1 tangent-space normal pointing straight out of the surface.
    AssetHandle<Texture> FlatNormalTexture(AssetManager2& assets);

    // Plain white, rough, non-metallic material for primitives that don't specify one.
    AssetHandle<MaterialAsset> DefaultMaterial(AssetManager2& assets, Shader2& pbrShader);
}
