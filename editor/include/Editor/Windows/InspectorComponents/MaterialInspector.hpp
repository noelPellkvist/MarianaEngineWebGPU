#pragma once

#include <Editor/Windows/InspectorComponents/AssetInspector.hpp>
#include <Mariana/Scene/ECS.hpp>

#include <Mariana/Rendering/MaterialAsset.hpp>

#include <array>
#include <cstddef>

class AssetManager2;
class MaterialAsset;
class UndoStack;

namespace InspectorComponents
{
    // Everything the material editor can change (factors, render state, texture slots), for undo.
    struct MaterialState
    {
        PBRMaterialProperties properties;
        bool doubleSided = false;
        AlphaMode alphaMode = AlphaMode::Opaque;
        std::array<MaterialAsset::TextureSlot, static_cast<size_t>(MaterialTexture::Count)> textures;

        static MaterialState Of(const MaterialAsset& material);
        void ApplyTo(MaterialAsset& material) const;
        bool operator==(const MaterialState& other) const;
    };

    // Turns what changed in `material` since `before` into one undo step ("Edit <name>"). While
    // `widgetHeld` (a drag or typing in progress) the change stays pending and is recorded once
    // it is let go. DrawMaterialEditor wraps its widgets in this.
    void RecordMaterialEdit(const AssetHandle<MaterialAsset>& material, const MaterialState& before, UndoStack& undo, bool widgetHeld);

    // The editor for one material: where it is saved (Save, Save As, or Extract materials for one
    // from a glTF), its factors and render state, and each texture slot with a thumbnail, the
    // texture's size and format, and its UV set and sampler. Images dropped on a slot load through
    // `assets`; clicking a thumbnail opens the texture. Edits change the material asset, so every
    // mesh using it, and each becomes one step in `undo` (a drag when it is let go).
    void DrawMaterialEditor(const AssetHandle<MaterialAsset>& material, AssetManager2& assets, const OpenAsset& openAsset, UndoStack& undo);

    // A section per material on `entity`'s mesh, or, for an entity without one (a model's root),
    // per material used below it. Each has an Open button for the inspector's asset view.
    void DrawEntityMaterials(Entity entity, const Scene& scene, AssetManager2& assets, const OpenAsset& openAsset, UndoStack& undo);

    // How many entities in `scene` draw with `material`.
    size_t CountMaterialUsers(const Scene& scene, const MaterialAsset* material);

    // Gives `entity` (and, with `includeDescendants`, everything below it) its own copy of
    // `material`, registered as "<name> (copy)". Returns the copy.
    AssetHandle<MaterialAsset> MakeMaterialUnique(Entity entity, bool includeDescendants, const Scene& scene,
                                                  AssetManager2& assets, const AssetHandle<MaterialAsset>& material);
}
