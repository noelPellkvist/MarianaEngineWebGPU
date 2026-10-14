#include <Editor/Windows/InspectorComponents/AssetInspector.hpp>

#include <Editor/GUI.hpp>
#include <Editor/Windows/InspectorComponents/MaterialInspector.hpp>
#include <Mariana/Animation/Animation.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Graphics/Texture.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <Mariana/Rendering/MaterialAsset.hpp>
#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    using namespace InspectorComponents;

    const char* const kTextureSlotNames[] = { "Base Color", "Normal", "Metallic Roughness", "Occlusion", "Emissive" };

    // Every entity in `scene` that `matches`.
    template <typename Fn>
    std::vector<Entity> FindEntities(const Scene& scene, Fn&& matches)
    {
        std::vector<Entity> found;
        auto visit = [&](Entity e) {
            if (matches(e))
                found.push_back(e);
        };
        scene.ForEachRoot([&](Entity root) {
            visit(root);
            scene.ForEachDescendant(root, visit);
        });
        return found;
    }

    // "Used by" list of entities; clicking one selects it.
    void DrawEntityList(const char* label, const std::vector<Entity>& entities, const SelectEntity& selectEntity)
    {
        ImGui::SeparatorText(label);
        if (entities.empty())
        {
            ImGui::TextDisabled("Nothing in the scene");
            return;
        }
        constexpr size_t kMaxShown = 50;
        for (size_t i = 0; i < std::min(entities.size(), kMaxShown); ++i)
        {
            Entity entity = entities[i];
            const char* name = entity.GetName();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(name && *name ? name : "Entity"))
                selectEntity(entity);
            ImGui::PopID();
        }
        if (entities.size() > kMaxShown)
            ImGui::TextDisabled("... and %zu more", entities.size() - kMaxShown);
    }

    void DrawCheckerboard(const ImVec2& min, const ImVec2& max)
    {
        constexpr float kCell = 8.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, IM_COL32(90, 90, 90, 255));
        drawList->PushClipRect(min, max, true);
        for (float y = min.y; y < max.y; y += kCell)
            for (float x = min.x + (static_cast<int>((y - min.y) / kCell) % 2) * kCell; x < max.x; x += 2.0f * kCell)
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + kCell, y + kCell), IM_COL32(140, 140, 140, 255));
        drawList->PopClipRect();
    }

    void DrawTextureViewer(Texture& texture, AssetManager2& assets, const OpenAsset& openAsset)
    {
        // One viewer, reset when another texture is opened.
        struct ViewState { const Texture* texture = nullptr; int mip = 0; bool r = true, g = true, b = true; bool checker = true; float zoom = 0.0f; };
        static ViewState view;
        if (view.texture != &texture)
            view = ViewState{ &texture };

        const TextureFormat format = texture.GetFormat();
        const bool srgb = format == TextureFormat::RGBA8UnormSrgb || format == TextureFormat::BGRA8UnormSrgb;
        const int mips = static_cast<int>(texture.GetMipLevelCount());
        ImGui::Text("%d x %d, %u mips", texture.GetWidth(), texture.GetHeight(), texture.GetMipLevelCount());
        ImGui::TextDisabled(srgb ? "Color (sRGB, decoded when sampled)" : "Data (linear: normals, masks, ...)");

        view.mip = std::clamp(view.mip, 0, std::max(mips - 1, 0));
        const int mipWidth = std::max(texture.GetWidth() >> view.mip, 1);
        const int mipHeight = std::max(texture.GetHeight() >> view.mip, 1);
        if (mips > 1)
            ImGui::SliderInt("Mip", &view.mip, 0, mips - 1, ("%d (" + std::to_string(mipWidth) + " x " + std::to_string(mipHeight) + ")").c_str());
        ImGui::Checkbox("R", &view.r);
        ImGui::SameLine();
        ImGui::Checkbox("G", &view.g);
        ImGui::SameLine();
        ImGui::Checkbox("B", &view.b);
        ImGui::SameLine();
        ImGui::Checkbox("Alpha checkerboard", &view.checker);
        ImGui::SliderFloat("Zoom", &view.zoom, 0.0f, 4.0f, view.zoom <= 0.0f ? "Fit" : "%.2fx");

        // Fit: as wide as the panel inside the frame's padding. Zoomed: scrolls past 640 px.
        const ImGuiStyle& style = ImGui::GetStyle();
        const bool fit = view.zoom <= 0.0f;
        const float available = ImGui::GetContentRegionAvail().x - 2.0f * style.WindowPadding.x - 2.0f;
        const ImVec2 size = fit
            ? ImVec2(available, available * mipHeight / mipWidth)
            : ImVec2(mipWidth * view.zoom, mipHeight * view.zoom);
        const float frame = 2.0f * style.WindowPadding.y + 2.0f;
        const float height = fit ? size.y + frame : std::min(size.y + style.ScrollbarSize + frame, 640.0f);
        if (ImGui::BeginChild("##preview", ImVec2(0.0f, height), ImGuiChildFlags_Borders,
                              fit ? ImGuiWindowFlags_NoScrollbar : ImGuiWindowFlags_HorizontalScrollbar))
        {
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + size.x, min.y + size.y);
            if (view.checker)
                DrawCheckerboard(min, max);
            else
                ImGui::GetWindowDrawList()->AddRectFilled(min, max, IM_COL32(0, 0, 0, 255));
            // The channel toggles tint the image: one channel on shows just that one.
            ImGui::ImageWithBg(GUI::TextureId(texture, view.mip), size, ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0),
                               ImVec4(view.r ? 1.0f : 0.0f, view.g ? 1.0f : 0.0f, view.b ? 1.0f : 0.0f, 1.0f));
        }
        ImGui::EndChild();

        ImGui::SeparatorText("Used by materials");
        bool any = false;
        assets.ForEach<MaterialAsset>([&](AssetID id, MaterialAsset& material) {
            for (uint32_t t = 0; t < static_cast<uint32_t>(MaterialTexture::Count); ++t)
            {
                if (material.GetTexture(static_cast<MaterialTexture>(t)).texture.Get().get() != &texture)
                    continue;
                const AssetMetadata* meta = assets.GetMetadata(id);
                const std::string label = (meta ? meta->name : std::string("Material")) + " (" + kTextureSlotNames[t] + ")";
                ImGui::PushID(&material);
                ImGui::PushID(static_cast<int>(t));
                if (ImGui::Selectable(label.c_str()))
                    openAsset(id);
                ImGui::PopID();
                ImGui::PopID();
                any = true;
            }
        });
        if (!any)
            ImGui::TextDisabled("None");
    }

    void DrawMeshInfo(IMesh& mesh, const Scene& scene, const SelectEntity& selectEntity)
    {
        ImGui::Text("Vertices: %zu", mesh.VertexCount());
        ImGui::Text("Indices: %zu (%s)", mesh.IndexCount(), mesh.IsUINT16() ? "uint16" : "uint32");
        ImGui::Text("Submeshes: %zu", mesh.submeshes.size());
        if (mesh.hasBounds)
        {
            ImGui::Text("Bounds min: %.2f, %.2f, %.2f", mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z);
            ImGui::Text("Bounds max: %.2f, %.2f, %.2f", mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z);
        }
        DrawEntityList("Used by", FindEntities(scene, [&](Entity e) {
            const MeshComponent* component = e.TryGet<MeshComponent>();
            return component && component->mesh.Get().get() == &mesh;
        }), selectEntity);
    }
}

namespace InspectorComponents
{
    void DrawAssetInspector(AssetID id, AssetManager2& assets, const Scene& scene,
                            const OpenAsset& openAsset, const SelectEntity& selectEntity)
    {
        const AssetMetadata* meta = assets.GetMetadata(id);
        if (!meta)
        {
            ImGui::TextDisabled("This asset no longer exists");
            return;
        }

        ImGui::TextUnformatted(meta->name.c_str());
        ImGui::TextDisabled("%s, %s, %u handles%s", AssetTypeName(meta->type), AssetStatusName(meta->status),
                            meta->refCount, meta->persistent ? ", persistent" : "");
        if (!meta->key.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", meta->key.c_str());
        ImGui::Separator();

        if (meta->status != AssetStatus::Loaded)
        {
            ImGui::TextDisabled("Not loaded");
            return;
        }

        switch (meta->type)
        {
            case AssetType::Material:
            {
                AssetHandle<MaterialAsset> material = assets.LoadAsset<MaterialAsset>(id);
                if (!material.Get())
                    break;
                if (ImGui::SmallButton("Duplicate"))
                {
                    AssetHandle<MaterialAsset> copy = assets.RegisterAsset<MaterialAsset>(
                        material.Get()->Clone(), AssetType::Material, meta->name + " (copy)");
                    openAsset(copy.GetID());
                }
                ImGui::SetItemTooltip("A new material with the same settings (assign it in a Mesh Renderer's slot).");
                DrawMaterialEditor(*material.Get(), assets, openAsset);
                MaterialAsset* raw = material.Get().get();
                DrawEntityList("Used by", FindEntities(scene, [&](Entity e) {
                    const MeshComponent* mesh = e.TryGet<MeshComponent>();
                    return mesh && std::any_of(mesh->materials.begin(), mesh->materials.end(),
                        [&](const AssetHandle<MaterialAsset>& m) { return m.Get().get() == raw; });
                }), selectEntity);
                break;
            }
            case AssetType::Texture:
            {
                AssetHandle<Texture> texture = assets.LoadAsset<Texture>(id);
                if (texture.Get())
                    DrawTextureViewer(*texture.Get(), assets, openAsset);
                break;
            }
            case AssetType::Mesh:
            {
                AssetHandle<IMesh> mesh = assets.LoadAsset<IMesh>(id);
                if (mesh.Get())
                    DrawMeshInfo(*mesh.Get(), scene, selectEntity);
                break;
            }
            case AssetType::Animation:
            {
                AssetHandle<Animation> animation = assets.LoadAsset<Animation>(id);
                if (animation.Get())
                {
                    ImGui::Text("Duration: %.2f s", animation.Get()->GetDuration());
                    ImGui::Text("Channels: %zu", animation.Get()->GetChannelCount());
                }
                break;
            }
            default:
                ImGui::TextDisabled("No viewer for this asset type yet");
                break;
        }
    }
}
