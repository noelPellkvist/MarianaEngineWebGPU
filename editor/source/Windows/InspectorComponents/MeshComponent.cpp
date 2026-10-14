#include <Editor/Windows/InspectorComponents/MeshComponent.hpp>
#include <Editor/UndoStack.hpp>

#include <Mariana/Rendering/MaterialAsset.hpp>
#include <Mariana/Graphics/Mesh.hpp>
#include <Mariana/Rendering/Components.hpp>
#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    const char* AssetName(const AssetMetadata* meta)
    {
        return meta ? meta->name.c_str() : "None";
    }

    // Picks the material a slot draws with: any loaded material, or the default one (empty).
    void DrawMaterialSlot(Entity entity, MeshComponent& meshComponent, uint32_t slot, AssetManager2& assets, UndoStack& undo)
    {
        const AssetHandle<MaterialAsset> current = slot < meshComponent.materials.size() ? meshComponent.materials[slot] : AssetHandle<MaterialAsset>{};
        const char* preview = current.Get() ? AssetName(current.GetMetadata()) : "Default Material";

        struct Choice { AssetID id; const MaterialAsset* material; std::string name; };
        auto setSlot = [entity, slot](const AssetHandle<MaterialAsset>& handle) {
            Entity e = entity;
            if (!e.IsValid() || !e.Has<MeshComponent>())
                return;
            MeshComponent& component = *e.Get<MeshComponent>();
            if (component.materials.size() <= slot)
                component.materials.resize(slot + 1);
            component.materials[slot] = handle;
        };
        auto assign = [&](AssetHandle<MaterialAsset> handle) {
            undo.Push("Assign material", [setSlot, old = current]() { setSlot(old); }, [setSlot, handle]() { setSlot(handle); });
            setSlot(handle);
        };

        const std::string label = "Element " + std::to_string(slot);
        if (ImGui::BeginCombo(label.c_str(), preview))
        {
            if (ImGui::Selectable("Default Material", !current.Get()))
                assign({});

            std::vector<Choice> choices;
            assets.ForEach<MaterialAsset>([&](AssetID id, MaterialAsset& material) {
                const AssetMetadata* meta = assets.GetMetadata(id);
                choices.push_back({ id, &material, meta ? meta->name : std::string("Material") });
            });
            std::sort(choices.begin(), choices.end(), [](const Choice& a, const Choice& b) { return a.name < b.name; });
            for (const Choice& choice : choices)
            {
                ImGui::PushID(choice.material);
                if (ImGui::Selectable(choice.name.c_str(), choice.material == current.Get().get()))
                    assign(assets.LoadAsset<MaterialAsset>(choice.id));
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }
}

namespace InspectorComponents
{
    void DrawMeshComponent(Entity& entity, AssetManager2& assets, UndoStack& undo)
    {
        if (!entity.Has<MeshComponent>())
            return;

        MeshComponent& meshComponent = *entity.Get<MeshComponent>();
        if (!ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::TextDisabled("Mesh");
        ImGui::SameLine(0, 16);
        ImGui::TextUnformatted(AssetName(meshComponent.mesh.GetMetadata()));

        std::shared_ptr<IMesh> mesh = meshComponent.mesh.Get();
        if (!mesh)
        {
            ImGui::TextDisabled("Mesh is not loaded");
            return;
        }

        bool castShadows = entity.HasTag<ShadowCasterTag>();
        if (ImGui::Checkbox("Cast Shadows", &castShadows))
        {
            auto setCasting = [target = entity](bool on) {
                Entity e = target;
                if (!e.IsValid())
                    return;
                if (on)
                    e.AddTag<ShadowCasterTag>();
                else
                    e.RemoveTag<ShadowCasterTag>();
            };
            undo.Push(castShadows ? "Cast shadows" : "Stop casting shadows",
                      [setCasting, castShadows]() { setCasting(!castShadows); },
                      [setCasting, castShadows]() { setCasting(castShadows); });
            setCasting(castShadows);
        }

        if (ImGui::TreeNode("Mesh Info"))
        {
            ImGui::Text("Vertices: %zu", mesh->VertexCount());
            ImGui::Text("Indices: %zu (%s)", mesh->IndexCount(), mesh->IsUINT16() ? "uint16" : "uint32");
            ImGui::Text("Submeshes: %zu", mesh->submeshes.size());
            ImGui::TreePop();
        }

        // One slot per material index the submeshes use (several submeshes may share one).
        std::vector<uint32_t> slots;
        for (const Submesh& submesh : mesh->submeshes)
            slots.push_back(submesh.materialIndex);
        std::sort(slots.begin(), slots.end());
        slots.erase(std::unique(slots.begin(), slots.end()), slots.end());

        ImGui::SeparatorText("Material Slots");
        for (uint32_t slot : slots)
        {
            ImGui::PushID(static_cast<int>(slot));
            DrawMaterialSlot(entity, meshComponent, slot, assets, undo);
            ImGui::PopID();
        }
    }
}
