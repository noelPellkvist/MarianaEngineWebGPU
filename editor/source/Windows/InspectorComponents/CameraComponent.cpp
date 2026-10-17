#include <Editor/Windows/InspectorComponents/CameraComponent.hpp>

#include <Mariana/Rendering/Components.hpp>
#include <imgui.h>

#include <algorithm>

namespace InspectorComponents
{
    void DrawCameraComponent(Entity& entity)
    {
        CameraComponent* camera = entity.Get<CameraComponent>();
        if (!camera || !ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::PushID("CameraComponent");
        ImGui::SliderFloat("Field of View", &camera->fov, 5.0f, 150.0f, "%.0f deg");
        ImGui::DragFloat("Near", &camera->nearClip, 0.01f, 0.001f, 100.0f, "%.3f m", ImGuiSliderFlags_Logarithmic);
        ImGui::DragFloat("Far", &camera->farClip, 1.0f, 1.0f, 100000.0f, "%.0f m", ImGuiSliderFlags_Logarithmic);
        camera->nearClip = std::max(camera->nearClip, 0.001f);
        camera->farClip = std::max(camera->farClip, camera->nearClip * 2.0f);
        ImGui::TextDisabled("Looks along its +Z axis; the Game view shows the first camera");
        ImGui::PopID();
    }
}
