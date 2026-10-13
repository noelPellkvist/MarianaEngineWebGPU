#include "Editor/Windows/LoadedAssetsWindow.hpp"

#include <Logger.hpp>
#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

LoadedAssetsWindow::LoadedAssetsWindow(GUI& gui, AssetManager2& assets)
    : EditorWindow("Loaded Assets", gui, false),
      m_assets(assets)
{
}

void LoadedAssetsWindow::Draw()
{
    if (!m_open)
        return;

    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        if (ImGui::Button("Unload Unused"))
        {
            size_t unloaded = m_assets.UnloadUnused();
            Logger::Info("Unloaded " + std::to_string(unloaded) + " unused assets");
        }
        ImGui::SameLine();
        ImGui::Checkbox("Show unloaded", &m_showUnloaded);

        std::vector<const AssetMetadata*> rows;
        size_t loadedCount = 0;
        for (const auto& [id, meta] : m_assets.GetAllMetadata())
        {
            if (meta.status == AssetStatus::Loaded)
                ++loadedCount;
            if (m_showUnloaded || meta.status != AssetStatus::Unloaded)
                rows.push_back(&meta);
        }
        std::sort(rows.begin(), rows.end(), [](const AssetMetadata* a, const AssetMetadata* b) {
            if (a->type != b->type)
                return a->type < b->type;
            return a->name < b->name;
        });

        ImGui::TextDisabled("%zu loaded, %zu known", loadedCount, m_assets.GetAllMetadata().size());

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("##assets", 4, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableHeadersRow();

            for (const AssetMetadata* meta : rows)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(meta->name.c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("ID: %s", meta->id.ToString().c_str());
                    if (!meta->key.empty())
                        ImGui::Text("Key: %s", meta->key.c_str());
                    if (meta->persistent)
                        ImGui::TextUnformatted("Persistent (never unloaded automatically)");
                    ImGui::EndTooltip();
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(AssetTypeName(meta->type));

                ImGui::TableSetColumnIndex(2);
                if (meta->status == AssetStatus::Loaded)
                    ImGui::TextUnformatted(AssetStatusName(meta->status));
                else
                    ImGui::TextDisabled("%s", AssetStatusName(meta->status));

                ImGui::TableSetColumnIndex(3);
                if (meta->persistent)
                    ImGui::Text("%u*", meta->refCount);
                else
                    ImGui::Text("%u", meta->refCount);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
