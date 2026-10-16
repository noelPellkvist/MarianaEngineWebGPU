#include <Mariana/Importers/ImageImporter.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <memory>
#include <string>

namespace ImageImporter
{
    namespace
    {
        std::string Key(const std::filesystem::path& file, bool srgb)
        {
            return AssetManager2::MakeKey(file) + (srgb ? "#srgb" : "#linear");
        }
    }

    AssetHandle<Texture> LoadTexture(AssetManager2& assets, const std::filesystem::path& file, bool srgb)
    {
        if (AssetHandle<Texture> existing = assets.Find<Texture>(Key(file, srgb)); existing.IsValid())
            return existing;

        Image::Pixels image;
        std::string error;
        if (!Image::Load(file, image, error))
        {
            Logger::Error("Failed to load image: " + file.string());
            return {};
        }
        return RegisterTexture(assets, file, srgb, image);
    }

    AssetHandle<Texture> RegisterTexture(AssetManager2& assets, const std::filesystem::path& file, bool srgb, const Image::Pixels& image)
    {
        if (image.Empty())
            return {};

        auto texture = std::make_shared<Texture>();
        texture->CreateTexture(image.width, image.height, srgb ? TextureFormat::RGBA8UnormSrgb : TextureFormat::RGBA8Unorm,
                               false, false, false, Texture::MipLevelCount(image.width, image.height));
        texture->UploadTextureWithMips(image.rgba.data(), image.rgba.size(), image.width, image.height);
        return assets.RegisterAsset<Texture>(texture, AssetType::Texture, file.filename().string(), Key(file, srgb), file);
    }
}
