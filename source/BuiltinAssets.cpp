#include <BuiltinAssets.hpp>
#include <MaterialAsset.hpp>
#include <Shader.hpp>

#include <cstdint>
#include <memory>

namespace
{
    AssetHandle<Texture> SolidTexture(AssetManager2& assets, const std::string& key, const std::string& name,
                                      const uint8_t (&rgba)[4], TextureFormat format)
    {
        if (AssetHandle<Texture> existing = assets.Find<Texture>(key); existing.IsValid())
            return existing;

        auto texture = std::make_shared<Texture>();
        texture->CreateTexture(1, 1, format);
        texture->UploadTexture(rgba, sizeof(rgba), 1, 1);

        AssetHandle<Texture> handle = assets.RegisterAsset<Texture>(texture, AssetType::Texture, name, key);
        assets.SetPersistent(handle.GetID(), true);
        return handle;
    }
}

namespace BuiltinAssets
{
    AssetHandle<Texture> WhiteTexture(AssetManager2& assets, bool srgb)
    {
        static const uint8_t white[4] = { 255, 255, 255, 255 };
        return srgb
            ? SolidTexture(assets, "builtin://texture/white_srgb", "White (sRGB)", white, TextureFormat::RGBA8UnormSrgb)
            : SolidTexture(assets, "builtin://texture/white_linear", "White (linear)", white, TextureFormat::RGBA8Unorm);
    }

    AssetHandle<Texture> FlatNormalTexture(AssetManager2& assets)
    {
        static const uint8_t flatNormal[4] = { 128, 128, 255, 255 };
        return SolidTexture(assets, "builtin://texture/flat_normal", "Flat Normal", flatNormal, TextureFormat::RGBA8Unorm);
    }

    AssetHandle<MaterialAsset> DefaultMaterial(AssetManager2& assets, Shader2& pbrShader)
    {
        const std::string key = "builtin://material/default";
        if (AssetHandle<MaterialAsset> existing = assets.Find<MaterialAsset>(key); existing.IsValid())
            return existing;

        AssetHandle<Texture> albedo = WhiteTexture(assets, true);
        AssetHandle<Texture> normal = FlatNormalTexture(assets);
        AssetHandle<Texture> data = WhiteTexture(assets, false);

        auto material = std::make_shared<MaterialAsset>();
        material->material.InitFromShader(pbrShader)
            .SetTexture("albedoMap", *albedo.Get())
            .SetTexture("normalMap", *normal.Get())
            .SetTexture("metallicRoughnessMap", *data.Get())
            .SetTexture("occlusionMap", *data.Get())
            .SetTexture("emissiveMap", *albedo.Get())
            .Build();
        material->textures = { albedo, normal, data };

        AssetHandle<MaterialAsset> handle = assets.RegisterAsset<MaterialAsset>(material, AssetType::Material, "Default Material", key);
        assets.SetPersistent(handle.GetID(), true);
        return handle;
    }
}
