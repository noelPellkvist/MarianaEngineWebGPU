#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Geometry/Primitives.hpp>

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

    AssetHandle<MaterialAsset> DefaultMaterial(AssetManager2& assets)
    {
        const std::string key = "builtin://material/default";
        if (AssetHandle<MaterialAsset> existing = assets.Find<MaterialAsset>(key); existing.IsValid())
            return existing;

        // No textures: the renderer binds neutral ones in their place.
        auto material = std::make_shared<MaterialAsset>();
        AssetHandle<MaterialAsset> handle = assets.RegisterAsset<MaterialAsset>(material, AssetType::Material, "Default Material", key);
        assets.SetPersistent(handle.GetID(), true);
        return handle;
    }

    AssetHandle<IMesh> PrimitiveMesh(AssetManager2& assets, BuiltinMesh shape)
    {
        const char* name = shape == BuiltinMesh::Cube ? "Cube" : shape == BuiltinMesh::Sphere ? "Sphere" : "Plane";
        const std::string key = std::string("builtin://mesh/") + name;
        if (AssetHandle<IMesh> existing = assets.Find<IMesh>(key); existing.IsValid())
            return existing;

        MeshSection section = shape == BuiltinMesh::Cube ? Primitives::Cube(1.0f)
                            : shape == BuiltinMesh::Sphere ? Primitives::Sphere(0.5f)
                            : Primitives::Plane(10.0f, 10);
        std::shared_ptr<IMesh> mesh = BuildMesh({ std::move(section) });
        if (!mesh)
            return {};
        return assets.RegisterAsset<IMesh>(mesh, AssetType::Mesh, name, key);
    }
}
