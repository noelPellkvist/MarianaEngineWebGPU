#pragma once

#include <imgui.h>
#include <glm/vec2.hpp>

// A pannable, zoomable area of an ImGui window that node editors draw on, in graph units. Begin it
// every frame over the area it covers: it draws a grid, pans while the middle or right mouse button
// drags, and zooms around the mouse with the wheel. Clicks are left to its user (hit tests with
// ToGraph); a right click that didn't drag is reported by RightClicked.
struct GraphCanvas
{
    glm::vec2 pan{ 0.0f }; // graph point at the area's top-left corner
    float zoom = 1.0f;

    // This frame's area on the screen.
    ImVec2 origin{ 0.0f, 0.0f };
    ImVec2 size{ 0.0f, 0.0f };
    bool hovered = false;

    // Adds an invisible item covering the area (so the window doesn't move when it's dragged).
    void Begin(const char* id, ImVec2 areaSize);
    void End();

    ImVec2 ToScreen(glm::vec2 p) const { return ImVec2(origin.x + (p.x - pan.x) * zoom, origin.y + (p.y - pan.y) * zoom); }
    glm::vec2 ToGraph(ImVec2 s) const { return glm::vec2((s.x - origin.x) / zoom + pan.x, (s.y - origin.y) / zoom + pan.y); }
    glm::vec2 MouseInGraph() const { return ToGraph(ImGui::GetMousePos()); }
    // Moves the view so `p` is in the middle of the area.
    void CenterOn(glm::vec2 p);

    bool RightClicked() const { return m_RightClicked; }
    bool Panning() const { return m_Panning; }

private:
    bool m_Panning = false;
    bool m_RightClicked = false;
};

namespace GraphDraw
{
    // Text at a graph point, sized `scale` times the font size and zoomed with the canvas.
    void Text(ImDrawList* drawList, const GraphCanvas& canvas, glm::vec2 at, ImU32 color, const char* text, float scale = 1.0f);
    // Width of that text in graph units.
    float TextWidth(const char* text, float scale = 1.0f);
    // A link from an output (going right) to an input (coming from the left).
    void Link(ImDrawList* drawList, ImVec2 from, ImVec2 to, ImU32 color, float thickness);
    // A straight arrow with its head in the middle.
    void Arrow(ImDrawList* drawList, ImVec2 from, ImVec2 to, ImU32 color, float thickness, float head);

    float DistanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b);
    float DistanceToLink(ImVec2 p, ImVec2 from, ImVec2 to);
}
