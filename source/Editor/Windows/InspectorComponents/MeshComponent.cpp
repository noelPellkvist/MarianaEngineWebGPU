#include "Editor/Windows/InspectorComponents/MeshComponent.hpp"

#include <MaterialAsset.hpp>
#include <Mesh.hpp>
#include <Renderer.hpp>
#include <Importers/ImageImporter.hpp>
#include <imgui.h>

namespace
{
    const char* AssetName(const AssetMetadata* meta)
    {
        return meta ? meta->name.c_str() : "None";
    }

    const char* const kTextureSlotNames[] = { "Base Color", "Normal", "Metallic Roughness", "Occlusion", "Emissive" };

    // Base color and emissive maps hold color (sRGB), the others data.
    bool IsColorSlot(MaterialTexture slot)
    {
        return slot == MaterialTexture::BaseColor || slot == MaterialTexture::Emissive;
    }

    bool AddressCombo(const char* label, SamplerAddress& address)
    {
        int value = static_cast<int>(address);
        if (!ImGui::Combo(label, &value, "Repeat\0Mirror Repeat\0Clamp\0"))
            return false;
        address = static_cast<SamplerAddress>(value);
        return true;
    }

    bool FilterCombo(const char* label, SamplerFilter& filter)
    {
        int value = static_cast<int>(filter);
        if (!ImGui::Combo(label, &value, "Nearest\0Linear\0"))
            return false;
        filter = static_cast<SamplerFilter>(value);
        return true;
    }

    // A texture slot: its texture (drop an image from the Assets window to replace it), UV set and sampler.
    void DrawTextureSlot(MaterialAsset& material, MaterialTexture slot, AssetManager2& assets)
    {
        const MaterialAsset::TextureSlot& current = material.GetTexture(slot);
        AssetHandle<Texture> texture = current.texture;
        SamplerSettings sampler = current.sampler;
        uint32_t uvSet = current.uvSet;
        bool changed = false;

        ImGui::PushID(static_cast<int>(slot));
        const bool open = ImGui::TreeNode("##slot", "%s: %s", kTextureSlotNames[static_cast<int>(slot)],
                                          texture.IsValid() ? AssetName(texture.GetMetadata()) : "None");
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARIANA_ASSET_IMAGE"))
            {
                AssetHandle<Texture> dropped = ImageImporter::LoadTexture(assets, static_cast<const char*>(payload->Data), IsColorSlot(slot));
                if (dropped.IsValid())
                {
                    texture = dropped;
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (open)
        {
            if (texture.IsValid() && ImGui::SmallButton("Clear"))
            {
                texture = {};
                changed = true;
            }
            int uv = static_cast<int>(uvSet);
            if (ImGui::Combo("UV Set", &uv, "TEXCOORD_0\0TEXCOORD_1\0"))
            {
                uvSet = static_cast<uint32_t>(uv);
                changed = true;
            }
            changed |= AddressCombo("Wrap U", sampler.addressU);
            changed |= AddressCombo("Wrap V", sampler.addressV);
            changed |= FilterCombo("Mag Filter", sampler.magFilter);
            changed |= FilterCombo("Min Filter", sampler.minFilter);
            changed |= ImGui::Checkbox("Mipmaps", &sampler.mipmaps);
            if (sampler.mipmaps)
                changed |= FilterCombo("Mip Filter", sampler.mipmapFilter);
            ImGui::TreePop();
        }
        ImGui::PopID();

        if (changed)
            material.SetTexture(slot, texture, sampler, uvSet);
    }

    // The material is an asset, so every mesh using it changes too.
    void DrawMaterial(MaterialAsset& material, AssetManager2& assets)
    {
        PBRMaterialProperties& p = material.properties;
        bool dirty = false;
        dirty |= ImGui::ColorEdit4("Base Color", &p.baseColor.x);
        dirty |= ImGui::SliderFloat("Metallic", &p.metallicFactor, 0.0f, 1.0f);
        dirty |= ImGui::SliderFloat("Roughness", &p.roughnessFactor, 0.0f, 1.0f);
        dirty |= ImGui::SliderFloat("Normal Strength", &p.normalMapStrength, 0.0f, 2.0f);
        dirty |= ImGui::SliderFloat("Occlusion Strength", &p.occlusionStrength, 0.0f, 1.0f);
        // HDR: values above 1 glow through the bloom.
        dirty |= ImGui::ColorEdit3("Emissive", &p.emissiveFactor.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        if (dirty)
            material.MarkDirty();

        int alphaMode = static_cast<int>(material.GetAlphaMode());
        float cutoff = p.alphaCutoff > 0.0f ? p.alphaCutoff : 0.5f;
        bool alphaChanged = ImGui::Combo("Alpha Mode", &alphaMode, "Opaque\0Mask\0Blend (drawn as Mask)\0");
        if (alphaMode != static_cast<int>(AlphaMode::Opaque))
            alphaChanged |= ImGui::SliderFloat("Alpha Cutoff", &cutoff, 0.0f, 1.0f);
        if (alphaChanged)
            material.SetAlphaMode(static_cast<AlphaMode>(alphaMode), cutoff);
        ImGui::Checkbox("Double Sided", &material.doubleSided);

        ImGui::SeparatorText("Textures");
        for (uint32_t t = 0; t < static_cast<uint32_t>(MaterialTexture::Count); ++t)
            DrawTextureSlot(material, static_cast<MaterialTexture>(t), assets);
    }
}

namespace InspectorComponents
{
    void DrawMeshComponent(Entity& entity, AssetManager2& assets)
    {
        if (!entity.Has<MeshComponent>())
            return;

        MeshComponent& meshComponent = *entity.Get<MeshComponent>();
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("Mesh");
            ImGui::SameLine(0, 16);
            ImGui::TextUnformatted(AssetName(meshComponent.mesh.GetMetadata()));

            std::shared_ptr<IMesh> mesh = meshComponent.mesh.Get();
            if (!mesh)
            {
                ImGui::TextDisabled("Mesh is not loaded");
                return;
            }

            ImGui::TextDisabled("Vertices");
            ImGui::SameLine(0, 16);
            ImGui::Text("%zu", mesh->VertexCount());

            ImGui::TextDisabled("Indices");
            ImGui::SameLine(0, 16);
            ImGui::Text("%zu", mesh->IndexCount());

            ImGui::TextDisabled("Index Format");
            ImGui::SameLine(0, 16);
            ImGui::TextUnformatted(mesh->IsUINT16() ? "uint16" : "uint32");

            ImGui::TextDisabled("Submeshes");
            ImGui::SameLine(0, 16);
            ImGui::Text("%zu", mesh->submeshes.size());

            ImGui::TextDisabled("Materials");
            for (size_t s = 0; s < mesh->submeshes.size(); ++s)
            {
                const uint32_t materialIndex = mesh->submeshes[s].materialIndex;
                const bool hasMaterial = materialIndex < meshComponent.materials.size() &&
                                         meshComponent.materials[materialIndex].Get();
                if (!hasMaterial)
                {
                    ImGui::BulletText("Default Material");
                    continue;
                }

                MaterialAsset& material = *meshComponent.materials[materialIndex].Get();
                ImGui::PushID(static_cast<int>(s));
                if (ImGui::TreeNode(AssetName(meshComponent.materials[materialIndex].GetMetadata())))
                {
                    DrawMaterial(material, assets);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }
}
