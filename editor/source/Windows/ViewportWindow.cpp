#include <Editor/Windows/ViewportWindow.hpp>
#include <Mariana/Graphics/Texture.hpp>

#include <imgui.h>

#include <algorithm>
#include <utility>

ViewportWindow::ViewportWindow(std::string name, GUI& gui)
    : EditorWindow(std::move(name), gui, true)
{
}

void ViewportWindow::Draw()
{
    m_Visible = false;
    m_Hovered = false;
    if (!m_open)
        return;

    // The first time it opens it docks in the middle, with the other views as tabs.
    ImGui::SetNextWindowDockID(m_gui.CentralNodeId(), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(960.0f, 540.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool shown = ImGui::Begin(m_name.c_str(), &m_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (shown)
    {
        const ImVec2 size = ImGui::GetContentRegionAvail();
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        m_PixelSize = glm::uvec2(static_cast<uint32_t>(std::max(size.x * scale.x, 0.0f)), static_cast<uint32_t>(std::max(size.y * scale.y, 0.0f)));
        m_Visible = m_PixelSize.x > 0 && m_PixelSize.y > 0;
        const ImVec2 min = ImGui::GetCursorScreenPos();
        m_Min = glm::vec2(min.x, min.y);
        m_Max = glm::vec2(min.x + size.x, min.y + size.y);
        if (m_Visible)
        {
            // Not a button: the gizmo only takes clicks when no item is hovered.
            Texture* texture = image ? image() : nullptr;
            if (texture)
                ImGui::Image(GUI::TextureId(*texture), size);
            else
                ImGui::Dummy(size);
            m_Hovered = ImGui::IsItemHovered();

            if (!texture && !emptyText.empty())
            {
                const ImVec2 textSize = ImGui::CalcTextSize(emptyText.c_str());
                const ImVec2 at(min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f);
                ImGui::GetWindowDrawList()->AddText(at, ImGui::GetColorU32(ImGuiCol_TextDisabled), emptyText.c_str());
            }
            if (overlay)
                overlay(*this);
        }
    }
    ImGui::End();
}

glm::ivec2 ViewportWindow::ToPixel(glm::vec2 screen, glm::uvec2 size) const
{
    const glm::vec2 extent = glm::max(m_Max - m_Min, glm::vec2(1.0f));
    const glm::vec2 uv = (screen - m_Min) / extent;
    return glm::ivec2(glm::floor(uv * glm::vec2(size)));
}
