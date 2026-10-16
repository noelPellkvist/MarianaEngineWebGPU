#pragma once
#include <filesystem>

#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/Image.hpp>

// Loads image files (anything stb_image reads) as mipmapped RGBA8 textures.
namespace ImageImporter
{
    // Color images (base color, emissive) are decoded from sRGB, data (normals, roughness, masks)
    // read as is; each color space is its own asset, keyed "<file>#srgb" or "<file>#linear", so
    // loading the file again reuses it. Returns an empty handle on failure.
    AssetHandle<Texture> LoadTexture(AssetManager2& assets, const std::filesystem::path& file, bool srgb);

    // Registers already decoded pixels as the texture of `file` (the asset LoadTexture would make),
    // e.g. an image decoded on another thread. Loading the file afterwards finds this one.
    AssetHandle<Texture> RegisterTexture(AssetManager2& assets, const std::filesystem::path& file, bool srgb, const Image::Pixels& image);
}
