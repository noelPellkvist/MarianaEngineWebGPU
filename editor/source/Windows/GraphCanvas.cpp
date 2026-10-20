#include <Editor/Windows/GraphCanvas.hpp>

#include <algorithm>
#include <cmath>

void GraphCanvas::Begin(const char* id, ImVec2 areaSize)
{
    origin = ImGui::GetCursorScreenPos();
    size = ImVec2(std::max(areaSize.x, 1.0f), std::max(areaSize.y, 1.0f));
    ImGui::InvisibleButton(id, size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size.x, origin.y + size.y);
    drawList->PushClipRect(origin, end, true);
    drawList->AddRectFilled(origin, end, ImGui::GetColorU32(ImGuiCol_FrameBg, 0.6f));

    // The grid: lines every 32 units, brighter every 4th, thinned out when zoomed far out.
    const float step = 32.0f * zoom;
    if (step > 6.0f)
    {
        const ImU32 minor = ImGui::GetColorU32(ImGuiCol_Border, 0.35f);
        const ImU32 major = ImGui::GetColorU32(ImGuiCol_Border, 0.8f);
        const float firstX = std::floor(pan.x / 32.0f), firstY = std::floor(pan.y / 32.0f);
        for (float i = firstX;; ++i)
        {
            const float x = origin.x + (i * 32.0f - pan.x) * zoom;
            if (x > end.x) break;
            drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), std::fmod(std::abs(i), 4.0f) == 0.0f ? major : minor);
        }
        for (float i = firstY;; ++i)
        {
            const float y = origin.y + (i * 32.0f - pan.y) * zoom;
            if (y > end.y) break;
            drawList->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), std::fmod(std::abs(i), 4.0f) == 0.0f ? major : minor);
        }
    }

    // Pan with the middle or right button; a right click that didn't move opens its user's menu.
    ImGuiIO& io = ImGui::GetIO();
    m_RightClicked = false;
    const bool dragging = ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 2.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4.0f));
    if (dragging)
    {
        pan.x -= io.MouseDelta.x / zoom;
        pan.y -= io.MouseDelta.y / zoom;
        m_Panning = true;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    {
        if (hovered && !m_Panning)
            m_RightClicked = true;
        m_Panning = false;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
        m_Panning = false;

    if (hovered && io.MouseWheel != 0.0f)
    {
        const glm::vec2 before = MouseInGraph();
        zoom = std::clamp(zoom * std::pow(1.15f, io.MouseWheel), 0.25f, 2.5f);
        const glm::vec2 after = MouseInGraph();
        pan += before - after;
    }
}

void GraphCanvas::End()
{
    ImGui::GetWindowDrawList()->PopClipRect();
}

void GraphCanvas::CenterOn(glm::vec2 p)
{
    pan = p - glm::vec2(size.x, size.y) * (0.5f / zoom);
}

namespace GraphDraw
{
    void Text(ImDrawList* drawList, const GraphCanvas& canvas, glm::vec2 at, ImU32 color, const char* text, float scale)
    {
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale * canvas.zoom, canvas.ToScreen(at), color, text);
    }

    float TextWidth(const char* text, float scale)
    {
        return ImGui::CalcTextSize(text).x * scale;
    }

    void Link(ImDrawList* drawList, ImVec2 from, ImVec2 to, ImU32 color, float thickness)
    {
        const float bend = std::max(std::abs(to.x - from.x) * 0.5f, 40.0f);
        drawList->AddBezierCubic(from, ImVec2(from.x + bend, from.y), ImVec2(to.x - bend, to.y), to, color, thickness);
    }

    void Arrow(ImDrawList* drawList, ImVec2 from, ImVec2 to, ImU32 color, float thickness, float head)
    {
        drawList->AddLine(from, to, color, thickness);
        const float dx = to.x - from.x, dy = to.y - from.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length < 1.0f)
            return;
        const ImVec2 d(dx / length, dy / length);
        const ImVec2 mid((from.x + to.x) * 0.5f + d.x * head * 0.5f, (from.y + to.y) * 0.5f + d.y * head * 0.5f);
        const ImVec2 n(-d.y, d.x);
        drawList->AddTriangleFilled(mid,
                                    ImVec2(mid.x - d.x * head + n.x * head * 0.6f, mid.y - d.y * head + n.y * head * 0.6f),
                                    ImVec2(mid.x - d.x * head - n.x * head * 0.6f, mid.y - d.y * head - n.y * head * 0.6f), color);
    }

    float DistanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b)
    {
        const float abx = b.x - a.x, aby = b.y - a.y;
        const float length2 = abx * abx + aby * aby;
        const float t = length2 > 0.0f ? std::clamp(((p.x - a.x) * abx + (p.y - a.y) * aby) / length2, 0.0f, 1.0f) : 0.0f;
        const float x = a.x + abx * t - p.x, y = a.y + aby * t - p.y;
        return std::sqrt(x * x + y * y);
    }

    float DistanceToLink(ImVec2 p, ImVec2 from, ImVec2 to)
    {
        const float bend = std::max(std::abs(to.x - from.x) * 0.5f, 40.0f);
        const ImVec2 c1(from.x + bend, from.y), c2(to.x - bend, to.y);
        float best = 1e9f;
        ImVec2 previous = from;
        for (int i = 1; i <= 16; ++i)
        {
            const float t = i / 16.0f, u = 1.0f - t;
            const ImVec2 point(u * u * u * from.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * to.x,
                               u * u * u * from.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * to.y);
            best = std::min(best, DistanceToSegment(p, previous, point));
            previous = point;
        }
        return best;
    }
}
