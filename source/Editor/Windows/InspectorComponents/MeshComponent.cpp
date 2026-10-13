#include "Editor/Windows/InspectorComponents/MeshComponent.hpp"

#include <MaterialAsset.hpp>
#include <Mesh.hpp>
#include <Renderer.hpp>
#include <imgui.h>

namespace
{
    const char* AssetName(const AssetMetadata* meta)
    {
        return meta ? meta->name.c_str() : "None";
    }
}

namespace InspectorComponents
{
    void DrawMeshComponent(Entity& entity)
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
                                         meshComponent.materials[materialIndex].IsValid();
                if (!hasMaterial)
                {
                    ImGui::BulletText("Default Material");
                    continue;
                }

                MaterialAsset& material = *meshComponent.materials[materialIndex].Get();
                ImGui::PushID(static_cast<int>(s));
                if (ImGui::TreeNode(AssetName(meshComponent.materials[materialIndex].GetMetadata())))
                {
                    // HDR: values above 1 glow through the bloom. The material is an asset, so every
                    // mesh using it changes too.
                    if (ImGui::ColorEdit3("Emissive", &material.properties.emissiveFactor.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float))
                        material.MarkDirty();
                    ImGui::Checkbox("Double Sided", &material.doubleSided);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }
}
