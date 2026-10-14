#pragma once
#include <filesystem>
#include <memory>
#include <string>

#include <Mariana/Assets/AssetManager.hpp>

class MaterialAsset;

// .material files: a MaterialAsset as JSON, its textures stored as image files.
//   {
//     "baseColor": [1, 1, 1, 1], "metallic": 0, "roughness": 1, "normalStrength": 1,
//     "occlusionStrength": 1, "emissive": [0, 0, 0], "alphaMode": "Opaque" | "Mask" | "Blend",
//     "alphaCutoff": 0.5, "doubleSided": false,
//     "textures": {
//       "baseColor": { "image": "albedo.png", "uvSet": 0, "wrapU": "Repeat", "wrapV": "Repeat",
//                      "magFilter": "Linear", "minFilter": "Linear", "mipFilter": "Linear", "mipmaps": true },
//       "normal": ..., "metallicRoughness": ..., "occlusion": ..., "emissive": ...
//     }
//   }
// Image paths are relative to the .material file, or resource paths when they start with "/".
// Everything is optional; missing values keep the defaults.
namespace MaterialFile
{
    // Reads a .material file (an OS path) into a new material; its images load through
    // ImageImporter in each slot's color space. nullptr (and `error`) on failure.
    std::shared_ptr<MaterialAsset> Load(AssetManager2& assets, const std::filesystem::path& file, std::string& error);

    // Writes `material` to `file`. Each texture is stored by its image file (AssetMetadata::source);
    // one embedded in a glTF is first written as a PNG next to `file`. A texture with no image file
    // is left out and reported in `error` (the save still succeeds).
    bool Save(const MaterialAsset& material, AssetManager2& assets, const std::filesystem::path& file, std::string& error);

    // Makes AssetManager2::Load<MaterialAsset>() handle .material files.
    void RegisterLoader(AssetManager2& assets);
}
