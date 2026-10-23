#pragma once
// Private to the engine: a material as JSON, as .material files hold it (MaterialFile) and as scene
// files carry the materials that have no file (SceneFile). Where a texture's image is found is the
// caller's business; the rest of each texture entry (UV set, sampler, transform) is written here.
#include <Mariana/Rendering/MaterialAsset.hpp>

#include <json.hpp>

#include <functional>
#include <memory>

namespace MaterialJson
{
    using Json = nlohmann::ordered_json;

    // In MaterialTexture order: the keys of the "textures" object.
    extern const char* const kSlotKeys[];

    // Base color and emissive maps hold color (sRGB), the others data.
    bool IsColorSlot(MaterialTexture slot);

    // Floats written with 6 decimals at most, so 0.1f is saved as 0.1.
    double Clean(float value);
    Json Array(const float* values, int count);

    // Fills in where a used slot's image is ("image", ...); false leaves the slot out.
    using WriteTexture = std::function<bool(MaterialTexture slot, const MaterialAsset::TextureSlot& texture, Json& entry)>;
    Json Write(const MaterialAsset& material, const WriteTexture& texture);

    // The texture a slot's entry names (an empty handle when it can't be had).
    using ReadTexture = std::function<AssetHandle<Texture>(MaterialTexture slot, const Json& entry)>;
    std::shared_ptr<MaterialAsset> Read(const Json& json, const ReadTexture& texture);
}
