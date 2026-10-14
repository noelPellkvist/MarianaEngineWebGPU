#include <Editor/Windows/InspectorComponents/MaterialInspector.hpp>

#include <Editor/GUI.hpp>
#include <Editor/UndoStack.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Importers/GLTFLoader.hpp>
#include <Mariana/Importers/ImageImporter.hpp>
#include <Mariana/Importers/MaterialFile.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    using InspectorComponents::OpenAsset;

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

    // The texture (clicking it returns true), or for an empty slot a swatch of what the renderer
    // samples instead.
    bool DrawThumbnail(Texture* texture, MaterialTexture slot)
    {
        const ImVec2 size(kThumbnailSize, kThumbnailSize);
        if (texture)
        {
            const bool clicked = ImGui::ImageButton("##thumbnail", GUI::TextureId(*texture), size);
            if (ImGui::BeginItemTooltip())
            {
                ImGui::Image(GUI::TextureId(*texture), ImVec2(kPreviewSize, kPreviewSize));
                ImGui::TextDisabled("Click to open");
                ImGui::EndTooltip();
            }
            return clicked;
        }
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        const ImU32 fallback = slot == MaterialTexture::Normal ? IM_COL32(128, 128, 255, 255) : IM_COL32(255, 255, 255, 255);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, fallback, 3.0f);
        drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
        ImGui::Dummy(size);
        return false;
    }

    // Points every slot of `entity` (and, with `descendants`, of everything below it) that uses
    // `from` at `to` instead.
    void ReplaceMaterial(Entity entity, bool descendants, const Scene& scene, const MaterialAsset* from, const AssetHandle<MaterialAsset>& to)
    {
        if (!entity.IsValid())
            return;
        auto replace = [&](Entity e) {
            MeshComponent* mesh = e.Has<MeshComponent>() ? e.Get<MeshComponent>() : nullptr;
            if (!mesh)
                return;
            for (AssetHandle<MaterialAsset>& slot : mesh->materials)
                if (slot.Get().get() == from)
                    slot = to;
        };
        replace(entity);
        if (descendants)
            scene.ForEachDescendant(entity, replace);
    }

    // The glTF a sub-asset key ("<file key>#<kind>/...") came from: the prefab's own path when
    // known (keeps its case), else the key's file part.
    std::filesystem::path ModelFile(AssetManager2& assets, const std::string& fileKey)
    {
        if (const AssetMetadata* prefab = assets.GetMetadata(assets.FindID(fileKey)); prefab && !prefab->path.empty())
            return prefab->path;
        return fileKey;
    }

    // Per material (by asset ID): its version when last loaded or saved, and the last save result.
    struct SaveState
    {
        uint32_t savedVersion = 0;
        std::string message;
    };

    SaveState& SaveStateOf(const AssetMetadata& meta)
    {
        static std::unordered_map<std::string, SaveState> states;
        return states[meta.id.ToString()];
    }

    // Where the material is kept, with Save / Extract materials / Save As.
    void DrawMaterialFile(const AssetHandle<MaterialAsset>& handle, AssetManager2& assets)
    {
        const AssetMetadata* meta = handle.GetMetadata();
        if (!meta)
            return;
        MaterialAsset& material = *handle.Get();
        SaveState& state = SaveStateOf(*meta);

        auto save = [&](const std::filesystem::path& file) {
            std::string error;
            const bool saved = MaterialFile::Save(material, assets, file, error);
            if (saved)
            {
                assets.SetSource(meta->id, file);
                state.savedVersion = material.Version();
            }
            state.message = saved ? (error.empty() ? "Saved " + file.filename().string() : error) : error;
            if (!error.empty())
                Logger::Warning(error);
        };

        std::filesystem::path file;
        if (meta->path.extension() == ".material")
            file = meta->path;
        else if (meta->source.extension() == ".material")
            file = meta->source;

        if (!file.empty())
        {
            static std::unordered_map<std::string, bool> seen;
            if (!seen[meta->id.ToString()])
            {
                seen[meta->id.ToString()] = true;
                state.savedVersion = material.Version();
            }
            const bool unsaved = state.savedVersion != material.Version();
            ImGui::TextDisabled("%s%s", file.filename().string().c_str(), unsaved ? " (unsaved)" : "");
            ImGui::SetItemTooltip("%s", file.string().c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Save"))
                save(file);
        }
        else if (const size_t at = meta->key.rfind("#material/"); at != std::string::npos)
        {
            const std::filesystem::path model = ModelFile(assets, meta->key.substr(0, at));
            ImGui::TextDisabled("From %s, not saved", model.filename().string().c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Extract materials"))
            {
                std::string error;
                const bool extracted = GLTF::GLTFLoader::ExtractMaterials(assets, model, error);
                state.message = extracted && error.empty() ? "Extracted next to " + model.filename().string() : error;
                if (!error.empty())
                    Logger::Warning(error);
            }
            ImGui::SetItemTooltip("Writes every material of %s to .material files next to it, so edits can be saved and are used when it loads.",
                                  model.filename().string().c_str());
        }
        else
        {
            ImGui::TextDisabled("Not saved");
        }

        ImGui::SameLine();
        static char saveName[64] = {};
        if (ImGui::SmallButton("Save As"))
        {
            const std::string name = meta->name.substr(meta->name.find_last_of('/') + 1);
            std::strncpy(saveName, name.c_str(), sizeof(saveName) - 1);
            ImGui::OpenPopup("SaveMaterialAs");
        }
        if (ImGui::BeginPopup("SaveMaterialAs"))
        {
            ImGui::TextUnformatted("Save to the project's Materials folder as");
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputText("##name", saveName, sizeof(saveName));
            ImGui::SameLine();
            ImGui::BeginDisabled(saveName[0] == '\0');
            if (ImGui::Button("Save"))
            {
                std::string name = saveName;
                for (char& c : name)
                    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != ' ')
                        c = '_';
                save(FileSystem::ResolveForWrite("/Materials/" + name + ".material"));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
        if (!state.message.empty())
            ImGui::TextDisabled("%s", state.message.c_str());
    }

    // A texture slot: thumbnail, texture name, size and format, and (folded) its UV set and
    // sampler. An image dragged from the Assets window replaces the texture.
    void DrawTextureSlot(MaterialAsset& material, MaterialTexture slot, AssetManager2& assets, const OpenAsset& openAsset)
    {
        const MaterialAsset::TextureSlot& current = material.GetTexture(slot);
        AssetHandle<Texture> texture = current.texture;
        SamplerSettings sampler = current.sampler;
        uint32_t uvSet = current.uvSet;
        Texture* gpuTexture = texture.Get().get();
        bool changed = false;

        ImGui::PushID(static_cast<int>(slot));
        ImGui::BeginGroup();
        if (DrawThumbnail(gpuTexture, slot))
            openAsset(texture.GetID());
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
    void DrawMaterialEditor(const AssetHandle<MaterialAsset>& handle, AssetManager2& assets, const OpenAsset& openAsset, UndoStack& undo)
    {
        if (!handle.Get())
            return;
        MaterialAsset& material = *handle.Get();
        DrawMaterialFile(handle, assets);
        const MaterialState before = MaterialState::Of(material);

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
            DrawTextureSlot(material, static_cast<MaterialTexture>(t), assets, openAsset);

        RecordMaterialEdit(handle, before, undo, ImGui::IsAnyItemActive());
    }

    MaterialState MaterialState::Of(const MaterialAsset& material)
    {
        MaterialState state;
        state.properties = material.properties;
        state.doubleSided = material.doubleSided;
        state.alphaMode = material.GetAlphaMode();
        for (size_t t = 0; t < state.textures.size(); ++t)
            state.textures[t] = material.GetTexture(static_cast<MaterialTexture>(t));
        return state;
    }

    void MaterialState::ApplyTo(MaterialAsset& material) const
    {
        for (size_t t = 0; t < textures.size(); ++t)
            material.SetTexture(static_cast<MaterialTexture>(t), textures[t].texture, textures[t].sampler, textures[t].uvSet);
        material.SetAlphaMode(alphaMode, properties.alphaCutoff > 0.0f ? properties.alphaCutoff : 0.5f);
        material.properties = properties;
        material.doubleSided = doubleSided;
        material.MarkDirty();
    }

    bool MaterialState::operator==(const MaterialState& other) const
    {
        if (!(properties == other.properties) || doubleSided != other.doubleSided || alphaMode != other.alphaMode)
            return false;
        for (size_t t = 0; t < textures.size(); ++t)
        {
            if (textures[t].texture.Get() != other.textures[t].texture.Get() || !(textures[t].sampler == other.textures[t].sampler) ||
                textures[t].uvSet != other.textures[t].uvSet)
                return false;
        }
        return true;
    }

    void RecordMaterialEdit(const AssetHandle<MaterialAsset>& handle, const MaterialState& before, UndoStack& undo, bool widgetHeld)
    {
        if (!handle.Get())
            return;
        // Changes collect here while a widget is held and become one step once nothing is.
        static std::unordered_map<const MaterialAsset*, MaterialState> pending;
        const MaterialAsset* material = handle.Get().get();
        const MaterialState after = MaterialState::Of(*material);
        if (!(after == before))
            pending.emplace(material, before);
        auto it = pending.find(material);
        if (it == pending.end() || widgetHeld)
            return;
        if (!(it->second == after))
        {
            const AssetMetadata* meta = handle.GetMetadata();
            undo.Push("Edit " + (meta ? meta->name : std::string("material")),
                      [handle, state = it->second]() { if (handle.Get()) state.ApplyTo(*handle.Get()); },
                      [handle, state = after]() { if (handle.Get()) state.ApplyTo(*handle.Get()); });
        }
        pending.erase(it);
    }

    void DrawEntityMaterials(Entity entity, const Scene& scene, AssetManager2& assets, const OpenAsset& openAsset, UndoStack& undo)
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
        AssetHandle<MaterialAsset> makeUnique;
        for (const AssetHandle<MaterialAsset>& handle : materials)
        {
            MaterialAsset& material = *handle.Get();
            ImGui::PushID(&material);
            const bool open = ImGui::CollapsingHeader(AssetName(handle.GetMetadata()), flags | ImGuiTreeNodeFlags_AllowOverlap);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
            if (ImGui::SmallButton("Open"))
                openAsset(handle.GetID());
            if (open)
            {
                const size_t users = CountMaterialUsers(scene, &material);
                ImGui::TextDisabled(users == 1 ? "Used by 1 mesh" : "Used by %zu meshes", users);
                // Only worth it when something outside the selection shares the material.
                size_t usersHere = 1;
                if (!ownMesh)
                {
                    usersHere = 0;
                    scene.ForEachDescendant(entity, [&](Entity e) {
                        const MeshComponent* mesh = e.TryGet<MeshComponent>();
                        if (mesh && std::any_of(mesh->materials.begin(), mesh->materials.end(),
                                [&](const AssetHandle<MaterialAsset>& m) { return m.Get().get() == &material; }))
                            ++usersHere;
                    });
                }
                if (users > usersHere)
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton(ownMesh ? "Make unique" : "Make unique to this model"))
                        makeUnique = handle;
                    ImGui::SetItemTooltip("Gives %s its own copy, so editing it leaves the other users alone.",
                                          ownMesh ? "this mesh" : "the meshes below this entity");
                }
                DrawMaterialEditor(handle, assets, openAsset, undo);
            }
            ImGui::PopID();
        }

        if (makeUnique.Get())
        {
            const AssetHandle<MaterialAsset> copy = MakeMaterialUnique(entity, !ownMesh, scene, assets, makeUnique);
            const Scene* scenePtr = &scene;
            const bool descendants = !ownMesh;
            undo.Push("Make material unique",
                      [=]() { ReplaceMaterial(entity, descendants, *scenePtr, copy.Get().get(), makeUnique); },
                      [=]() { ReplaceMaterial(entity, descendants, *scenePtr, makeUnique.Get().get(), copy); });
        }
    }

    AssetHandle<MaterialAsset> MakeMaterialUnique(Entity entity, bool includeDescendants, const Scene& scene,
                                                  AssetManager2& assets, const AssetHandle<MaterialAsset>& material)
    {
        if (!material.Get())
            return {};
        const AssetMetadata* meta = material.GetMetadata();
        AssetHandle<MaterialAsset> copy = assets.RegisterAsset<MaterialAsset>(
            material.Get()->Clone(), AssetType::Material, (meta ? meta->name : std::string("Material")) + " (copy)");
        ReplaceMaterial(entity, includeDescendants, scene, material.Get().get(), copy);
        return copy;
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
