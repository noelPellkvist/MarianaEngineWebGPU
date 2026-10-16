#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <Mariana/Scene/Prefab.hpp>
#include <Mariana/Assets/AssetManager.hpp>

namespace GLTF
{

// A glTF file read and decoded (images as RGBA8, meshes ready to upload) without the GPU or an
// AssetManager2, so it can be done on another thread. LoadGLTF turns it into assets.
struct ParsedGLTF;

struct ParseOptions
{
    // Images are halved until neither side is larger (0 = full size), e.g. for previews.
    uint32_t maxImageSize = 0;
};

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

        // The two halves of LoadGLTF. Parse touches neither the GPU nor any shared state (any
        // thread); nullptr and `error` on failure. LoadGLTF (main thread) uploads what was parsed,
        // using up its meshes, and logs the warnings parsing collected.
        static std::shared_ptr<ParsedGLTF> Parse(const std::filesystem::path& file, std::string& error, const ParseOptions& options = {});
        static std::shared_ptr<Prefab> LoadGLTF(AssetManager2& assets, ParsedGLTF& parsed);

        // The files besides the glTF itself that its content comes from: external buffers and
        // images, and the .material files its materials are (or would be) extracted to, whether
        // they exist or not.
        static std::vector<std::filesystem::path> Dependencies(const ParsedGLTF& parsed);

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
