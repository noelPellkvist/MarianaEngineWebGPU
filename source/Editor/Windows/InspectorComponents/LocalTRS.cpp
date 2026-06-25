#include "Editor/Windows/InspectorComponents/LocalTRS.hpp"

#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace
{
    bool DragOrInputFloat(const char* id, float* v, float speed, const char* fmt, float width)
    {
        struct State { bool editing = false; };
        static std::unordered_map<ImGuiID, State> s;

        ImGuiID iid = ImGui::GetID(id);
        State& st = s[iid];
        bool changed = false;

        ImGui::SetNextItemWidth(width);

        if (!st.editing)
        {
            changed |= ImGui::DragFloat(id, v, speed, 0.0f, 0.0f, fmt);

            if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())
            {
                st.editing = true;
                ImGui::SetKeyboardFocusHere(0);
            }
        }
        else
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), fmt, static_cast<double>(*v));

            bool submit = ImGui::InputText(id, buf, IM_ARRAYSIZE(buf),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            if (submit || ImGui::IsItemDeactivated())
            {
                *v = std::strtof(buf, nullptr);
                st.editing = false;
                changed = true;
            }
        }

        return changed;
    }

    void DrawVec3Row(const char* label, float v[3], float resetX, float resetY, float resetZ, float speed = 0.1f)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);

        ImGui::PushID(label);
        float lineH = ImGui::GetFrameHeight();
        float btnW = lineH;
        float fullW = ImGui::GetContentRegionAvail().x;
        float fieldW = (fullW - btnW * 3.0f - ImGui::GetStyle().ItemInnerSpacing.x * 6.0f) / 3.0f;

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 80, 80, 255));
        if (ImGui::Button("X", ImVec2(btnW, lineH))) v[0] = resetX;
        ImGui::SameLine();
        DragOrInputFloat("##X", &v[0], speed, "%.3f", fieldW);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(110, 190, 110, 255));
        if (ImGui::Button("Y", ImVec2(btnW, lineH))) v[1] = resetY;
        ImGui::SameLine();
        DragOrInputFloat("##Y", &v[1], speed, "%.3f", fieldW);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 140, 220, 255));
        if (ImGui::Button("Z", ImVec2(btnW, lineH))) v[2] = resetZ;
        ImGui::SameLine();
        DragOrInputFloat("##Z", &v[2], speed, "%.3f", fieldW);
        ImGui::PopStyleColor();

        ImGui::PopID();
    }
}

namespace InspectorComponents
{
    void DrawLocalTRS(Entity& entity)
    {
        if (!entity.Has<LocalTRS>())
            return;

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            LocalTRS localTRS = *(entity.Get<LocalTRS>());
            float* pos = localTRS.pos;
            float* rotDeg = localTRS.rot_euler;
            float* scl = localTRS.scl;

            if (ImGui::BeginTable("##transform_table", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody))
            {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("values", ImGuiTableColumnFlags_WidthStretch);

                DrawVec3Row("Position", pos, 0.0f, 0.0f, 0.0f, 0.1f);
                DrawVec3Row("Rotation", rotDeg, 0.0f, 0.0f, 0.0f, 0.5f);
                DrawVec3Row("Scale", scl, 1.0f, 1.0f, 1.0f, 0.05f);

                entity.SetPosition(pos[0], pos[1], pos[2]);
                entity.SetRotationEuler(rotDeg[0], rotDeg[1], rotDeg[2]);
                entity.SetScale(scl[0], scl[1], scl[2]);
                ImGui::EndTable();
            }
        }
    }
}