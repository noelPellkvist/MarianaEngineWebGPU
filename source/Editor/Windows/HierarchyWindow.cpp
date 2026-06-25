#include "Editor/Windows/HierarchyWindow.hpp"

#include <Logger.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>

HierarchyWindow::HierarchyWindow(
    GUI& gui,
    const Scene& scene,
    const uint64_t& selectedEntityID,
    SelectEntityCallback selectEntity,
    SpawnGlbCallback spawnGlb)
    : EditorWindow("Hierarchy", gui, true),
      m_scene(scene),
      m_selectedEntityID(selectedEntityID),
      m_selectEntity(std::move(selectEntity)),
      m_spawnGlb(std::move(spawnGlb))
{
}

void HierarchyWindow::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        DrawDropTarget();

        m_scene.ForEachRoot([&](Entity e) {
            DrawEntityNode(e);
        });
    }
    ImGui::End();
}

void HierarchyWindow::DrawDropTarget()
{
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    const ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    const ImVec2 hierarchyMin(windowPos.x + contentMin.x, windowPos.y + contentMin.y);
    const ImVec2 hierarchyMax(windowPos.x + contentMax.x, windowPos.y + contentMax.y);
    const bool hierarchyHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
    const bool glbDragging = activePayload && activePayload->IsDataType("MARIANA_ASSET_GLB");

    if (hierarchyHovered && glbDragging)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 8.0f);
        dl->AddRectFilled(hierarchyMin, hierarchyMax, ImGui::GetColorU32(ImGuiCol_Header, 0.10f + 0.08f * pulse), 6.0f);
        dl->AddRect(hierarchyMin, hierarchyMax, ImGui::GetColorU32(ImGuiCol_HeaderActive), 6.0f, 0, 2.0f + pulse);

        const char* hint = "Drop GLB to spawn prefab";
        ImVec2 hintSize = ImGui::CalcTextSize(hint);
        ImVec2 hintPos = ImVec2(hierarchyMin.x + ((hierarchyMax.x - hierarchyMin.x) - hintSize.x) * 0.5f, hierarchyMin.y + 10.0f);
        dl->AddText(hintPos, ImGui::GetColorU32(ImGuiCol_Text), hint);
    }

    if (ImGui::BeginDragDropTargetCustom(ImRect(hierarchyMin, hierarchyMax), ImGui::GetID("##hierarchy_glb_drop_target")))
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARIANA_ASSET_GLB"))
        {
            const char* droppedPath = static_cast<const char*>(payload->Data);
            if (droppedPath && *droppedPath)
            {
                try
                {
                    m_spawnGlb(std::filesystem::path(droppedPath));
                }
                catch (...)
                {
                    Logger::Error(std::string("Failed to handle dropped GLB: ") + droppedPath);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void HierarchyWindow::DrawEntityNode(Entity e)
{
    const char* name = e.GetName();
    if (!name || !*name)
        name = "<error_name>";

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        (e.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf) |
        (m_selectedEntityID == e.RawId() ? ImGuiTreeNodeFlags_Selected : 0);

    ImGui::PushID((ImGuiID)(uintptr_t)e.RawId());
    bool open = ImGui::TreeNodeEx("label", flags, "%s", name);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        m_selectEntity(e.RawId());

    if (ImGui::BeginPopupContextItem("entity_ctx"))
    {
        if (ImGui::MenuItem("Select"))
            m_selectEntity(e.RawId());
        ImGui::EndPopup();
    }

    if (open)
    {
        m_scene.ForEachChild(e, [&](Entity child) {
            DrawEntityNode(child);
        });
        ImGui::TreePop();
    }

    ImGui::PopID();
}