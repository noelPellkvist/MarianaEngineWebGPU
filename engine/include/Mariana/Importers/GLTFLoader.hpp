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
};

} // namespace GLTF
