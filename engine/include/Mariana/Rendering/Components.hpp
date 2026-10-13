#pragma once
#include <vector>

#include <Mariana/Assets/AssetManager.hpp>

class IMesh;
class MaterialAsset;

// Draws `mesh` with the scene renderer.
struct MeshComponent {
    AssetHandle<IMesh> mesh;
    // Material for each submesh, indexed by Submesh::materialIndex. Missing or empty entries
    // render with the default material.
    std::vector<AssetHandle<MaterialAsset>> materials;
};

// The entity's mesh casts shadows.
struct ShadowCasterTag {};
