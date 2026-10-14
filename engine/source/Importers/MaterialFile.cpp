#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Importers/ImageImporter.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>

#include <json.hpp>

#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
    using Json = nlohmann::ordered_json;

    // In MaterialTexture order.
    const char* const kSlotKeys[] = { "baseColor", "normal", "metallicRoughness", "occlusion", "emissive" };

    // Base color and emissive maps hold color (sRGB), the others data.
    bool IsColorSlot(MaterialTexture slot)
    {
        return slot == MaterialTexture::BaseColor || slot == MaterialTexture::Emissive;
    }

    // Floats written with 6 decimals at most, so 0.1f is saved as 0.1.
    double Clean(float value)
    {
        return std::round(static_cast<double>(value) * 1e6) / 1e6;
    }

    Json Array(const float* values, int count)
    {
        Json array = Json::array();
        for (int i = 0; i < count; ++i)
            array.push_back(Clean(values[i]));
        return array;
    }

    template <typename Vec>
    void ReadArray(const Json& object, const char* key, Vec& out, int count)
    {
        const auto it = object.find(key);
        if (it == object.end() || !it->is_array())
            return;
        for (int i = 0; i < count && i < static_cast<int>(it->size()); ++i)
            out[i] = (*it)[i].template get<float>();
    }

    const char* AddressName(SamplerAddress address)
    {
        return address == SamplerAddress::MirrorRepeat ? "MirrorRepeat" : address == SamplerAddress::ClampToEdge ? "Clamp" : "Repeat";
    }

    SamplerAddress ParseAddress(const std::string& name)
    {
        return name == "MirrorRepeat" ? SamplerAddress::MirrorRepeat : name == "Clamp" ? SamplerAddress::ClampToEdge : SamplerAddress::Repeat;
    }

    const char* FilterName(SamplerFilter filter)
    {
        return filter == SamplerFilter::Nearest ? "Nearest" : "Linear";
    }

    SamplerFilter ParseFilter(const std::string& name)
    {
        return name == "Nearest" ? SamplerFilter::Nearest : SamplerFilter::Linear;
    }

    const char* AlphaModeName(AlphaMode mode)
    {
        return mode == AlphaMode::Mask ? "Mask" : mode == AlphaMode::Blend ? "Blend" : "Opaque";
    }
}

namespace MaterialFile
{
    std::shared_ptr<MaterialAsset> Load(AssetManager2& assets, const std::filesystem::path& file, std::string& error)
    {
        Json json;
        try
        {
            std::ifstream stream(file, std::ios::binary);
            if (!stream)
            {
                error = "Could not open " + file.string();
                return nullptr;
            }
            std::ostringstream text;
            text << stream.rdbuf();
            json = Json::parse(text.str());
        }
        catch (const std::exception& e)
        {
            error = file.string() + ": " + e.what();
            return nullptr;
        }

        auto material = std::make_shared<MaterialAsset>();
        PBRMaterialProperties& p = material->properties;
        ReadArray(json, "baseColor", p.baseColor, 4);
        p.metallicFactor = json.value("metallic", p.metallicFactor);
        p.roughnessFactor = json.value("roughness", p.roughnessFactor);
        p.normalMapStrength = json.value("normalStrength", p.normalMapStrength);
        p.occlusionStrength = json.value("occlusionStrength", p.occlusionStrength);
        ReadArray(json, "emissive", p.emissiveFactor, 3);
        material->doubleSided = json.value("doubleSided", false);

        const std::string alphaMode = json.value("alphaMode", std::string("Opaque"));
        material->SetAlphaMode(alphaMode == "Mask" ? AlphaMode::Mask : alphaMode == "Blend" ? AlphaMode::Blend : AlphaMode::Opaque,
                               json.value("alphaCutoff", 0.5f));

        if (const auto textures = json.find("textures"); textures != json.end() && textures->is_object())
        {
            for (uint32_t t = 0; t < static_cast<uint32_t>(MaterialTexture::Count); ++t)
            {
                const auto entry = textures->find(kSlotKeys[t]);
                if (entry == textures->end() || !entry->is_object())
                    continue;
                const MaterialTexture slot = static_cast<MaterialTexture>(t);
                const std::string image = entry->value("image", std::string());
                if (image.empty())
                    continue;
                const std::filesystem::path imageFile = image[0] == '/' ? FileSystem::Resolve(image) : file.parent_path() / image;
                AssetHandle<Texture> texture = imageFile.empty() ? AssetHandle<Texture>{} : ImageImporter::LoadTexture(assets, imageFile, IsColorSlot(slot));
                if (!texture.IsValid())
                    Logger::Warning(file.string() + ": could not load " + image);

                SamplerSettings sampler;
                sampler.addressU = ParseAddress(entry->value("wrapU", std::string("Repeat")));
                sampler.addressV = ParseAddress(entry->value("wrapV", std::string("Repeat")));
                sampler.magFilter = ParseFilter(entry->value("magFilter", std::string("Linear")));
                sampler.minFilter = ParseFilter(entry->value("minFilter", std::string("Linear")));
                sampler.mipmapFilter = ParseFilter(entry->value("mipFilter", std::string("Linear")));
                sampler.mipmaps = entry->value("mipmaps", true);
                material->SetTexture(slot, texture, sampler, entry->value("uvSet", 0u));
            }
        }
        return material;
    }

    bool Save(const MaterialAsset& material, AssetManager2& assets, const std::filesystem::path& file, std::string& error)
    {
        const PBRMaterialProperties& p = material.properties;
        Json json;
        json["baseColor"] = Array(&p.baseColor.x, 4);
        json["metallic"] = Clean(p.metallicFactor);
        json["roughness"] = Clean(p.roughnessFactor);
        json["normalStrength"] = Clean(p.normalMapStrength);
        json["occlusionStrength"] = Clean(p.occlusionStrength);
        json["emissive"] = Array(&p.emissiveFactor.x, 3);
        json["alphaMode"] = AlphaModeName(material.GetAlphaMode());
        json["alphaCutoff"] = Clean(p.alphaCutoff > 0.0f ? p.alphaCutoff : 0.5f);
        json["doubleSided"] = material.doubleSided;

        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);

        Json textures = Json::object();
        for (uint32_t t = 0; t < static_cast<uint32_t>(MaterialTexture::Count); ++t)
        {
            const MaterialAsset::TextureSlot& slot = material.GetTexture(static_cast<MaterialTexture>(t));
            if (!slot.texture.IsValid())
                continue;

            // The image file: where the texture came from, or a PNG written from its glTF.
            std::filesystem::path image = GLTF::GLTFLoader::ExtractImage(assets, slot.texture.GetID(), file.parent_path(), error);
            if (image.empty())
            {
                const AssetMetadata* meta = slot.texture.GetMetadata();
                error += std::string(error.empty() ? "" : "\n") + "Left out the " + kSlotKeys[t] + " texture " +
                         (meta ? meta->name : std::string()) + ": it has no image file";
                continue;
            }
            std::filesystem::path relative = std::filesystem::relative(image, file.parent_path(), ec);
            if (ec || relative.empty())
                relative = image;

            Json entry;
            entry["image"] = relative.generic_string();
            entry["uvSet"] = slot.uvSet;
            entry["wrapU"] = AddressName(slot.sampler.addressU);
            entry["wrapV"] = AddressName(slot.sampler.addressV);
            entry["magFilter"] = FilterName(slot.sampler.magFilter);
            entry["minFilter"] = FilterName(slot.sampler.minFilter);
            entry["mipFilter"] = FilterName(slot.sampler.mipmapFilter);
            entry["mipmaps"] = slot.sampler.mipmaps;
            textures[kSlotKeys[t]] = entry;
        }
        json["textures"] = textures;

        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Could not write " + file.string();
            return false;
        }
        stream << json.dump(2) << "\n";
        return static_cast<bool>(stream);
    }

    void RegisterLoader(AssetManager2& assets)
    {
        assets.RegisterLoader<MaterialAsset>(AssetType::Material, { ".material" },
            [](AssetManager2& manager, const AssetMetadata& meta) {
                std::string error;
                std::shared_ptr<MaterialAsset> material = Load(manager, meta.path, error);
                if (!material)
                    Logger::Error(error);
                return material;
            });
    }
}
