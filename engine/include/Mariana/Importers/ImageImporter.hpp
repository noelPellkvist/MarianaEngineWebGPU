#pragma once
#include <filesystem>

#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Assets/AssetManager.hpp>

// Loads image files (anything stb_image reads) as mipmapped RGBA8 textures.
namespace ImageImporter
{
    // Color images (base color, emissive) are decoded from sRGB, data (normals, roughness, masks)
    // read as is; each color space is its own asset, keyed "<file>#srgb" or "<file>#linear", so
    // loading the file again reuses it. Returns an empty handle on failure.
    AssetHandle<Texture> LoadTexture(AssetManager2& assets, const std::filesystem::path& file, bool srgb);
}
