#include <Mariana/Importers/ImageImporter.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <stb_image.h>

#include <memory>

namespace ImageImporter
{
    AssetHandle<Texture> LoadTexture(AssetManager2& assets, const std::filesystem::path& file, bool srgb)
    {
        const std::string key = AssetManager2::MakeKey(file) + (srgb ? "#srgb" : "#linear");
        if (AssetHandle<Texture> existing = assets.Find<Texture>(key); existing.IsValid())
            return existing;

        int width = 0, height = 0, channels = 0;
        stbi_uc* pixels = stbi_load(file.string().c_str(), &width, &height, &channels, 4);
        if (!pixels)
        {
            Logger::Error("Failed to load image: " + file.string());
            return {};
        }

        auto texture = std::make_shared<Texture>();
        texture->CreateTexture(width, height, srgb ? TextureFormat::RGBA8UnormSrgb : TextureFormat::RGBA8Unorm,
                               false, false, false, Texture::MipLevelCount(width, height));
        texture->UploadTextureWithMips(pixels, static_cast<size_t>(width) * height * 4, width, height);
        stbi_image_free(pixels);

        return assets.RegisterAsset<Texture>(texture, AssetType::Texture, file.filename().string(), key, file);
    }
}
