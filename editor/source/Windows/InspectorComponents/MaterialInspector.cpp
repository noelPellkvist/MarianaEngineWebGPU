#include <Editor/Windows/InspectorComponents/MaterialInspector.hpp>

#include <Editor/GUI.hpp>
#include <Mariana/Importers/ImageImporter.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <imgui.h>

#include <algorithm>
#include <vector>

namespace
{
    constexpr float kThumbnailSize = 56.0f;
    constexpr float kPreviewSize = 256.0f;

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

    const char* FormatName(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::RGBA8Unorm: return "RGBA8";
            case TextureFormat::RGBA8UnormSrgb: return "RGBA8 sRGB";
            case TextureFormat::BGRA8Unorm: return "BGRA8";
            case TextureFormat::BGRA8UnormSrgb: return "BGRA8 sRGB";
            case TextureFormat::RGBA16Float: return "RGBA16F";
            case TextureFormat::R8Unorm: return "R8";
            default: return "other format";
        }
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

    // The texture, or for an empty slot a swatch of what the renderer samples instead.
    void DrawThumbnail(Texture* texture, MaterialTexture slot)
    {
        const ImVec2 size(kThumbnailSize, kThumbnailSize);
        if (texture)
        {
            ImGui::Image(GUI::TextureId(*texture), size);
            if (ImGui::BeginItemTooltip())
            {
                ImGui::Image(GUI::TextureId(*texture), ImVec2(kPreviewSize, kPreviewSize));
                ImGui::EndTooltip();
            }
            return;
        }
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        const ImU32 fallback = slot == MaterialTexture::Normal ? IM_COL32(128, 128, 255, 255) : IM_COL32(255, 255, 255, 255);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, fallback, 3.0f);
        drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
        ImGui::Dummy(size);
    }

    // A texture slot: thumbnail, texture name, size and format, and (folded) its UV set and
    // sampler. An image dragged from the Assets window replaces the texture.
    void DrawTextureSlot(MaterialAsset& material, MaterialTexture slot, AssetManager2& assets)
    {
        const MaterialAsset::TextureSlot& current = material.GetTexture(slot);
        AssetHandle<Texture> texture = current.texture;
        SamplerSettings sampler = current.sampler;
        uint32_t uvSet = current.uvSet;
        Texture* gpuTexture = texture.Get().get();
        bool changed = false;

        ImGui::PushID(static_cast<int>(slot));
        ImGui::BeginGroup();
        DrawThumbnail(gpuTexture, slot);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(kTextureSlotNames[static_cast<int>(slot)]);
        if (gpuTexture)
        {
            ImGui::TextDisabled("%s", AssetName(texture.GetMetadata()));
            ImGui::TextDisabled("%d x %d, %s, %u mips", gpuTexture->GetWidth(), gpuTexture->GetHeight(),
                                FormatName(gpuTexture->GetFormat()), gpuTexture->GetMipLevelCount());
            if (ImGui::SmallButton("Clear"))
            {
                texture = {};
                changed = true;
            }
        }
        else
        {
            ImGui::TextDisabled(slot == MaterialTexture::Normal ? "None (flat)" : "None (white)");
            ImGui::TextDisabled("Drop an image here");
        }
        ImGui::EndGroup();
        ImGui::EndGroup();

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

        if (ImGui::TreeNode("Sampling"))
        {
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
}

namespace InspectorComponents
{
    void DrawMaterialEditor(MaterialAsset& material, AssetManager2& assets)
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

    void DrawEntityMaterials(Entity entity, const Scene& scene, AssetManager2& assets)
    {
        // The entity's own materials, or for one without a mesh (a model's root) those below it.
        std::vector<AssetHandle<MaterialAsset>> materials;
        auto collect = [&](Entity e) {
            const MeshComponent* mesh = e.TryGet<MeshComponent>();
            if (!mesh)
                return;
            for (const AssetHandle<MaterialAsset>& handle : mesh->materials)
            {
                const bool known = std::any_of(materials.begin(), materials.end(),
                    [&](const AssetHandle<MaterialAsset>& m) { return m.Get() == handle.Get(); });
                if (handle.Get() && !known)
                    materials.push_back(handle);
            }
        };
        const bool ownMesh = entity.TryGet<MeshComponent>() != nullptr;
        if (ownMesh)
            collect(entity);
        else
            scene.ForEachDescendant(entity, collect);
        if (materials.empty())
            return;

        // Open by default only when there are few (a building can have dozens).
        const ImGuiTreeNodeFlags flags = ownMesh && materials.size() <= 3 ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        ImGui::SeparatorText(ownMesh ? "Materials" : "Materials used below");
        for (const AssetHandle<MaterialAsset>& handle : materials)
        {
            MaterialAsset& material = *handle.Get();
            ImGui::PushID(&material);
            if (ImGui::CollapsingHeader(AssetName(handle.GetMetadata()), flags))
            {
                const size_t users = CountMaterialUsers(scene, &material);
                ImGui::TextDisabled(users == 1 ? "Used by 1 mesh" : "Used by %zu meshes", users);
                DrawMaterialEditor(material, assets);
            }
            ImGui::PopID();
        }
    }

    size_t CountMaterialUsers(const Scene& scene, const MaterialAsset* material)
    {
        size_t count = 0;
        auto visit = [&](Entity e) {
            const MeshComponent* mesh = e.TryGet<MeshComponent>();
            if (!mesh)
                return;
            const bool uses = std::any_of(mesh->materials.begin(), mesh->materials.end(),
                [&](const AssetHandle<MaterialAsset>& m) { return m.Get().get() == material; });
            if (uses)
                ++count;
        };
        scene.ForEachRoot([&](Entity root) {
            visit(root);
            scene.ForEachDescendant(root, visit);
        });
        return count;
    }
}
