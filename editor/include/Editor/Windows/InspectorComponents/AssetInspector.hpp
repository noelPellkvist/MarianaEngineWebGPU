#pragma once

#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <functional>

class UndoStack;

namespace InspectorComponents
{
    // Opens an asset in the inspector (materials, textures, meshes, ...).
    using OpenAsset = std::function<void(AssetID)>;
    // Selects an entity (and shows it in the inspector).
    using SelectEntity = std::function<void(Entity)>;

    // The inspector's asset view: what the asset is, then by type the material editor, a texture
    // viewer (zoom, mip, channels) or mesh/animation info, and what in the scene uses it.
    void DrawAssetInspector(AssetID id, AssetManager2& assets, const Scene& scene,
                            const OpenAsset& openAsset, const SelectEntity& selectEntity, UndoStack& undo);
}
