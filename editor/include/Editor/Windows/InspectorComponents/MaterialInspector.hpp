#pragma once

#include <Mariana/Scene/ECS.hpp>

#include <cstddef>

class AssetManager2;
class MaterialAsset;

namespace InspectorComponents
{
    // The editor for one material: its factors and render state, and each texture slot with a
    // thumbnail, the texture's size and format, and its UV set and sampler. Images dropped on a
    // slot load through `assets`. Edits change the material asset, so every mesh using it.
    void DrawMaterialEditor(MaterialAsset& material, AssetManager2& assets);

    // A section per material on `entity`'s mesh, or, for an entity without one (a model's root),
    // per material used below it.
    void DrawEntityMaterials(Entity entity, const Scene& scene, AssetManager2& assets);

    // How many entities in `scene` draw with `material`.
    size_t CountMaterialUsers(const Scene& scene, const MaterialAsset* material);
}
