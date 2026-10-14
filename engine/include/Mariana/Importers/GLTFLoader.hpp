#pragma once
#include <filesystem>
#include <memory>

#include <Mariana/Scene/Prefab.hpp>
#include <Mariana/Assets/AssetManager.hpp>

namespace GLTF
{

// Imports glTF 2.0 files as engine assets: meshes (via MeshBuilder), MaterialAssets, textures,
// skins and animations, and a prefab of the scene's nodes. Nothing it makes is glTF-specific, so
// the renderer treats imported content like any other.
class GLTFLoader
{
    public:
        // Parses a .gltf/.glb file into a prefab. Its textures, materials, meshes and animations
        // are registered with `assets` under "<file>#<kind>/<index>" keys, so loading the file
        // again reuses whichever of them are still loaded. Returns nullptr on failure.
        static std::shared_ptr<Prefab> LoadGLTF(AssetManager2& assets, const std::filesystem::path& file);

        // Makes AssetManager2::Load<Prefab>() handle .gltf and .glb files.
        static void RegisterLoader(AssetManager2& assets);

        // The image file of a texture: its AssetMetadata::source, or for an image embedded in a
        // glTF this loader read, a PNG written into `folder` (which then becomes its source).
        // Empty when there is none; `error` is set if writing the image failed.
        static std::filesystem::path ExtractImage(AssetManager2& assets, AssetID texture, const std::filesystem::path& folder, std::string& error);

        // Writes the materials of a loaded glTF as .material files (ExtractedMaterialFile), current
        // edits included, with embedded images as PNGs next to them, and links each material to its
        // file (its source). From then on loading the glTF uses those files instead of its own
        // materials. Problems that didn't stop it are reported in `error`.
        static bool ExtractMaterials(AssetManager2& assets, const std::filesystem::path& file, std::string& error);

        // Where material `index` (called `name`) of `file` is extracted to: "<file name>.materials/
        // <index>_<name>.material" next to it.
        static std::filesystem::path ExtractedMaterialFile(const std::filesystem::path& file, size_t index, const std::string& name);
};

} // namespace GLTF
