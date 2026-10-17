#pragma once

#include <Editor/Windows/EditorWindow.hpp>

#include <glm/glm.hpp>

#include <functional>
#include <string>

class Texture;

// A window showing a rendered view (the scene from the editor camera, the game from its camera):
// the image fills the window, and the window's size is the size the view should render at.
class ViewportWindow final : public EditorWindow
{
public:
    ViewportWindow(std::string name, GUI& gui);
    void Draw() override;

    // What to show; nullptr shows `emptyText` instead.
    std::function<Texture*()> image;
    std::string emptyText;
    // Drawn over the image, inside the window (gizmos, picking).
    std::function<void(ViewportWindow&)> overlay;

    // Shown this frame (open, not collapsed or behind another tab) and not empty.
    bool IsVisible() const { return m_Visible; }
    bool IsHovered() const { return m_Hovered; }
    // What the view should render: the image's size in framebuffer pixels.
    glm::uvec2 PixelSize() const { return m_PixelSize; }
    // Where the image is on screen, in ImGui coordinates.
    glm::vec2 ImageMin() const { return m_Min; }
    glm::vec2 ImageMax() const { return m_Max; }
    // A point on screen (ImGui coordinates) as a pixel of an image of `size` pixels shown here.
    glm::ivec2 ToPixel(glm::vec2 screen, glm::uvec2 size) const;

private:
    bool m_Visible = false;
    bool m_Hovered = false;
    glm::uvec2 m_PixelSize{ 0 };
    glm::vec2 m_Min{ 0.0f };
    glm::vec2 m_Max{ 0.0f };
};
