#include <Mariana/Rendering/BuiltinAssets.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Geometry/Primitives.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

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

    AssetHandle<Texture> WaterNormalTexture(AssetManager2& assets)
    {
        const std::string key = "builtin://texture/water_normal";
        if (AssetHandle<Texture> existing = assets.Find<Texture>(key); existing.IsValid())
            return existing;

        // Ripples: a sum of sines whose wave numbers are whole, so the map tiles; the normal leans
        // down each slope.
        struct Wave { float kx, ky, amplitude, phase; };
        const Wave waves[] = { { 3, 1, 1.0f, 0.0f }, { 1, 4, 0.8f, 1.3f }, { 5, -2, 0.5f, 2.1f },
                               { -2, 7, 0.35f, 0.7f }, { 8, 3, 0.25f, 4.0f }, { -6, 9, 0.18f, 5.2f } };
        constexpr int size = 256;
        constexpr float twoPi = 6.28318530718f;
        std::vector<uint8_t> pixels(size * size * 4);
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float u = (x + 0.5f) / size, v = (y + 0.5f) / size;
                float dx = 0.0f, dy = 0.0f;
                for (const Wave& w : waves)
                {
                    const float c = std::cos(twoPi * (w.kx * u + w.ky * v) + w.phase) * w.amplitude * twoPi * 0.02f;
                    dx += c * w.kx;
                    dy += c * w.ky;
                }
                const float length = std::sqrt(dx * dx + dy * dy + 1.0f);
                uint8_t* p = &pixels[(y * size + x) * 4];
                p[0] = static_cast<uint8_t>(std::lround((-dx / length * 0.5f + 0.5f) * 255.0f));
                p[1] = static_cast<uint8_t>(std::lround((-dy / length * 0.5f + 0.5f) * 255.0f));
                p[2] = static_cast<uint8_t>(std::lround((1.0f / length * 0.5f + 0.5f) * 255.0f));
                p[3] = 255;
            }
        }
        auto texture = std::make_shared<Texture>();
        texture->CreateTexture(size, size, TextureFormat::RGBA8Unorm, false, false, false, 9);
        texture->UploadTextureWithMips(pixels.data(), pixels.size(), size, size);
        AssetHandle<Texture> handle = assets.RegisterAsset<Texture>(texture, AssetType::Texture, "Water Normal", key);
        assets.SetPersistent(handle.GetID(), true);
        return handle;
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
        const char* const names[] = { "Cube", "Sphere", "Plane", "Cylinder", "Capsule" };
        const char* name = names[static_cast<int>(shape)];
        const std::string key = std::string("builtin://mesh/") + name;
        if (AssetHandle<IMesh> existing = assets.Find<IMesh>(key); existing.IsValid())
            return existing;

        MeshSection section = shape == BuiltinMesh::Cube ? Primitives::Cube(1.0f)
                            : shape == BuiltinMesh::Sphere ? Primitives::Sphere(0.5f)
                            : shape == BuiltinMesh::Cylinder ? Primitives::Cylinder(0.5f, 1.0f)
                            : shape == BuiltinMesh::Capsule ? Primitives::Capsule(0.5f, 2.0f)
                            : Primitives::Plane(10.0f, 10);
        std::shared_ptr<IMesh> mesh = BuildMesh({ std::move(section) });
        if (!mesh)
            return {};
        return assets.RegisterAsset<IMesh>(mesh, AssetType::Mesh, name, key);
    }

    AssetID Resolve(AssetManager2& assets, const std::string& key)
    {
        if (AssetID existing = assets.FindID(key); existing.IsValid() && assets.IsLoaded(existing))
            return existing;
        if (key == "builtin://texture/white_srgb")
            return WhiteTexture(assets, true).GetID();
        if (key == "builtin://texture/white_linear")
            return WhiteTexture(assets, false).GetID();
        if (key == "builtin://texture/flat_normal")
            return FlatNormalTexture(assets).GetID();
        if (key == "builtin://texture/water_normal")
            return WaterNormalTexture(assets).GetID();
        if (key == "builtin://material/default")
            return DefaultMaterial(assets).GetID();
        const char* const meshes[] = { "Cube", "Sphere", "Plane", "Cylinder", "Capsule" };
        for (int i = 0; i < 5; ++i)
            if (key == std::string("builtin://mesh/") + meshes[i])
                return PrimitiveMesh(assets, static_cast<BuiltinMesh>(i)).GetID();
        return {};
    }

    Entity InstantiatePrimitive(Scene& scene, AssetManager2& assets, BuiltinMesh shape, const char* name, const glm::vec3& color)
    {
        AssetHandle<IMesh> mesh = PrimitiveMesh(assets, shape);
        if (!mesh.IsValid())
            return {};
        auto material = std::make_shared<MaterialAsset>();
        material->properties.baseColor = glm::vec4(color, 1.0f);
        material->MarkDirty();
        AssetHandle<MaterialAsset> handle = assets.RegisterAsset<MaterialAsset>(material, AssetType::Material, std::string(name) + " Material");
        Entity entity = scene.Instantiate(name);
        entity.Add<MeshComponent>(MeshComponent{ mesh, { handle } }).AddTag<ShadowCasterTag>();
        return entity;
    }
}
