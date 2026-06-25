#include "Editor/Windows/InspectorComponents/MeshComponent.hpp"

#include <AssetManager.hpp>
#include <Renderer.hpp>
#include <imgui.h>

namespace InspectorComponents
{
    void DrawMeshComponent(Entity& entity)
    {
        if (!entity.Has<MeshComponent>())
            return;

        MeshComponent& meshComponent = *entity.Get<MeshComponent>();
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("Mesh Index");
            ImGui::SameLine(0, 16);
            ImGui::Text("%u", meshComponent.meshIndex);

            const bool validIndex = meshComponent.meshIndex < AssetManager::LoadedMeshes.size();
            ImGui::TextDisabled("Valid");
            ImGui::SameLine(0, 16);
            ImGui::TextUnformatted(validIndex ? "Yes" : "No");

            if (!validIndex)
                return;

            IMesh* mesh = AssetManager::LoadedMeshes[meshComponent.meshIndex].get();
            if (!mesh)
            {
                ImGui::TextDisabled("Mesh data is null");
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
        }
    }
}