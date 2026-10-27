#include <Editor/Windows/ProfilerWindow.hpp>

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string_view>

namespace
{
    constexpr size_t kAverageFrames = 120;

    bool Contains(std::string_view text, std::string_view part)
    {
        return text.find(part) != std::string_view::npos;
    }

    // A colour per area, so the timeline reads at a glance.
    ImU32 ColorOf(std::string_view name)
    {
        if (name == "Present")
            return IM_COL32(90, 90, 98, 255);
        if (Contains(name, "Physics"))
            return IM_COL32(70, 160, 105, 255);
        if (Contains(name, "Animat"))
            return IM_COL32(150, 105, 200, 255);
        if (Contains(name, "Script") || Contains(name, "."))
            return IM_COL32(215, 145, 55, 255);
        if (Contains(name, "UI") || name == "GUI")
            return IM_COL32(60, 155, 175, 255);
        if (Contains(name, "Encode") || Contains(name, "view") || Contains(name, "Scene data") || Contains(name, "Transforms") || name == "Render")
            return IM_COL32(75, 115, 205, 255);
        return IM_COL32(115, 115, 135, 255);
    }

    // A tick spacing of 1, 2 or 5 times a power of ten, about `pixels` apart.
    double TickStep(double span, float width, float pixels)
    {
        const double rough = span * pixels / std::max(width, 1.0f);
        const double power = std::pow(10.0, std::floor(std::log10(rough)));
        for (double m : { 1.0, 2.0, 5.0, 10.0 })
            if (m * power >= rough)
                return m * power;
        return 10.0 * power;
    }
}

ProfilerWindow::ProfilerWindow(GUI& gui, std::function<void(const std::string& trace)> saveTrace)
    : EditorWindow("Profiler", gui, false), m_SaveTrace(std::move(saveTrace))
{
}

const Profiler::Frame* ProfilerWindow::Selected() const
{
    const std::deque<Profiler::Frame>& frames = Profiler::Frames();
    if (frames.empty())
        return nullptr;
    if (m_Selected != 0)
    {
        for (auto it = frames.rbegin(); it != frames.rend(); ++it)
            if (it->index == m_Selected)
                return &*it;
    }
    // Live: the newest frame whose GPU times have come back (they arrive a few frames late).
    size_t looked = 0;
    for (auto it = frames.rbegin(); it != frames.rend() && looked < 8; ++it, ++looked)
        if (it->gpuMs >= 0.0 || !Profiler::GpuTimed())
            return &*it;
    return &frames.back();
}

double ProfilerWindow::WaitMs(const Profiler::Frame& frame)
{
    if (m_Waits.size() > 4 * Profiler::kFrames)
        m_Waits.clear();
    auto [it, added] = m_Waits.emplace(frame.index, 0.0);
    if (added)
        for (const Profiler::Sample& sample : frame.samples)
            if (sample.thread == 0 && sample.depth == 0 && std::strcmp(sample.name, "Present") == 0)
                it->second += sample.end - sample.start;
    return it->second;
}

void ProfilerWindow::Refresh(const Profiler::Frame& frame)
{
    const bool live = m_Selected == 0 && !Profiler::Paused();
    if (m_Average == m_RefreshedAverage && (frame.index == m_RefreshedFrame || (live && Profiler::Now() - m_RefreshedAt < 250.0)))
        return;
    m_RefreshedFrame = frame.index;
    m_RefreshedAverage = m_Average;
    m_RefreshedAt = Profiler::Now();

    const std::vector<const Profiler::Frame*> window = Window(kAverageFrames);
    m_Stats = Profiler::Summarize(m_Average ? window : std::vector<const Profiler::Frame*>{ &frame });

    m_GpuAverages.clear();
    std::map<std::string, int> gpuFrames;
    for (const Profiler::Frame* f : window)
    {
        std::map<std::string, double> sums;
        for (const Profiler::GpuPass& pass : f->gpu)
            sums[pass.name] += pass.ms;
        for (const auto& [name, ms] : sums)
        {
            GpuAverage& a = m_GpuAverages[name];
            a.average += ms;
            a.max = std::max(a.max, ms);
            ++gpuFrames[name];
        }
    }
    for (auto& [name, a] : m_GpuAverages)
        a.average /= gpuFrames[name];

    m_Counters.clear();
    for (const Profiler::Counter& counter : frame.counters)
    {
        CounterHistory& history = m_Counters[counter.name];
        for (const Profiler::Frame* f : window)
        {
            double value = 0.0;
            for (const Profiler::Counter& c : f->counters)
                if (std::strcmp(c.name, counter.name) == 0)
                    value = c.value;
            history.values.push_back(static_cast<float>(value));
            history.average += value;
        }
        if (!window.empty())
            history.average /= window.size();
    }
}

std::vector<const Profiler::Frame*> ProfilerWindow::Window(size_t count) const
{
    std::vector<const Profiler::Frame*> window;
    const Profiler::Frame* last = Selected();
    if (!last)
        return window;
    for (const Profiler::Frame& frame : Profiler::Frames())
        if (frame.index <= last->index && frame.index + count > last->index)
            window.push_back(&frame);
    return window;
}

void ProfilerWindow::Draw()
{
    if (!m_open)
        return;
    ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        const Profiler::Frame* frame = Selected();
        DrawToolbar(frame);
        DrawFrameGraph();
        frame = Selected();
        if (frame)
        {
            Refresh(*frame);
            DrawTimeline(*frame);
            if (ImGui::BeginTabBar("##profilerTabs"))
            {
                if (ImGui::BeginTabItem("Scopes"))
                {
                    DrawScopes(*frame);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("GPU"))
                {
                    DrawGpu(*frame);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Counters"))
                {
                    DrawCounters(*frame);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        else
        {
            ImGui::TextDisabled("No frames recorded.");
        }
    }
    ImGui::End();
}

void ProfilerWindow::DrawToolbar(const Profiler::Frame* frame)
{
    const bool paused = Profiler::Paused();
    if (ImGui::Button(paused ? "Resume" : "Pause"))
    {
        Profiler::SetPaused(!paused);
        if (paused)
            m_Selected = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        Profiler::Clear();
        m_Selected = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save trace") && m_SaveTrace)
    {
        std::vector<const Profiler::Frame*> frames;
        for (const Profiler::Frame& f : Profiler::Frames())
            frames.push_back(&f);
        m_SaveTrace(Profiler::Trace(frames));
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Every kept frame as a Chrome trace (JSON): open it in ui.perfetto.dev or chrome://tracing.");

    // Step through frames while paused.
    if (paused && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && frame)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && frame->index > 1)
            m_Selected = frame->index - 1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            m_Selected = frame->index + 1;
    }

    if (frame)
    {
        const double waiting = WaitMs(*frame);
        ImGui::SameLine();
        ImGui::Text("  Frame %llu   CPU %.2f ms (%.2f waiting for the display)", static_cast<unsigned long long>(frame->index), frame->cpuMs, waiting);
        ImGui::SameLine();
        if (frame->gpuMs >= 0.0)
            ImGui::Text("  GPU %.2f ms", frame->gpuMs);
        else
            ImGui::TextDisabled("  GPU -");
        if (paused)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "  PAUSED (left/right steps)");
        }
    }
}

void ProfilerWindow::DrawFrameGraph()
{
    const std::deque<Profiler::Frame>& frames = Profiler::Frames();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = std::max(ImGui::GetContentRegionAvail().x, 50.0f);
    const float height = 96.0f;
    ImGui::InvisibleButton("##frameGraph", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(22, 22, 26, 255));
    if (frames.empty())
        return;

    // The scale fits the work (not the display wait), at least two 60 Hz frames.
    double top = 33.4;
    for (const Profiler::Frame& frame : frames)
        top = std::max(top, std::max(frame.cpuMs - WaitMs(frame), frame.gpuMs) * 1.1);
    top = std::min(top, 250.0);
    const auto yOf = [&](double ms) { return origin.y + height - static_cast<float>(std::min(ms, top) / top) * height; };

    const float slot = width / static_cast<float>(Profiler::kFrames);
    const float barWidth = std::max(slot - (slot > 3.0f ? 1.0f : 0.0f), 1.0f);
    const size_t count = frames.size();
    const Profiler::Frame* selected = Selected();
    ImVec2 lastGpu(-1.0f, 0.0f);
    int hoveredIndex = -1;
    for (size_t i = 0; i < count; ++i)
    {
        const Profiler::Frame& frame = frames[i];
        const float x = origin.x + width - static_cast<float>(count - i) * slot;
        const double waiting = WaitMs(frame);
        const double work = frame.cpuMs - waiting;
        const bool isSelected = selected && frame.index == selected->index;
        const bool isHovered = hovered && ImGui::GetMousePos().x >= x && ImGui::GetMousePos().x < x + slot;
        if (isHovered)
            hoveredIndex = static_cast<int>(i);
        const ImU32 color = isSelected ? IM_COL32(255, 210, 90, 255) : isHovered ? IM_COL32(140, 190, 255, 255) : IM_COL32(85, 135, 220, 255);
        draw->AddRectFilled(ImVec2(x, yOf(work)), ImVec2(x + barWidth, origin.y + height), color);
        draw->AddRectFilled(ImVec2(x, yOf(frame.cpuMs)), ImVec2(x + barWidth, yOf(work)), IM_COL32(70, 70, 78, 255));
        if (frame.gpuMs >= 0.0)
        {
            const ImVec2 point(x + barWidth * 0.5f, yOf(frame.gpuMs));
            if (lastGpu.x >= 0.0f)
                draw->AddLine(lastGpu, point, IM_COL32(255, 140, 60, 255), 1.5f);
            lastGpu = point;
        }
        else
            lastGpu.x = -1.0f;
    }

    for (double guide : { 1000.0 / 60.0, 1000.0 / 30.0 })
    {
        if (guide > top)
            continue;
        const float y = yOf(guide);
        draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + width, y), IM_COL32(255, 255, 255, 50));
        char label[32];
        std::snprintf(label, sizeof(label), "%.1f ms", guide);
        draw->AddText(ImVec2(origin.x + 4.0f, y - ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 110), label);
    }
    draw->AddText(ImVec2(origin.x + width - 150.0f, origin.y + 2.0f), IM_COL32(255, 255, 255, 120), "CPU work / wait   GPU");
    draw->AddLine(ImVec2(origin.x + width - 22.0f, origin.y + 9.0f), ImVec2(origin.x + width - 6.0f, origin.y + 9.0f), IM_COL32(255, 140, 60, 255), 2.0f);

    if (hoveredIndex >= 0)
    {
        const Profiler::Frame& frame = frames[hoveredIndex];
        const double waiting = WaitMs(frame);
        ImGui::BeginTooltip();
        ImGui::Text("Frame %llu", static_cast<unsigned long long>(frame.index));
        ImGui::Text("CPU %.2f ms: %.2f working, %.2f waiting for the display", frame.cpuMs, frame.cpuMs - waiting, waiting);
        if (frame.gpuMs >= 0.0)
            ImGui::Text("GPU %.2f ms", frame.gpuMs);
        ImGui::TextDisabled("Click to look at it (pauses)");
        ImGui::EndTooltip();
        if (clicked)
        {
            m_Selected = frame.index;
            Profiler::SetPaused(true);
        }
    }
}

void ProfilerWindow::DrawTimeline(const Profiler::Frame& frame)
{
    const float rowHeight = ImGui::GetTextLineHeight() + 4.0f;
    const float rulerHeight = ImGui::GetTextLineHeight() + 4.0f;

    // Rows: per thread, a label row and then a row per depth; then the GPU's passes.
    std::map<uint16_t, uint16_t> depths; // thread -> deepest
    for (const Profiler::Sample& sample : frame.samples)
        depths[sample.thread] = std::max<uint16_t>(depths[sample.thread], sample.depth);
    std::map<uint16_t, float> rowOf; // thread -> its first scope row's y offset
    float rows = rulerHeight;
    for (const auto& [thread, deepest] : depths)
    {
        rows += rowHeight; // its name
        rowOf[thread] = rows;
        rows += rowHeight * (deepest + 1);
    }
    const float gpuRow = rows + rowHeight;
    if (!frame.gpu.empty())
        rows = gpuRow + rowHeight;

    const float width = std::max(ImGui::GetContentRegionAvail().x, 50.0f);
    const float height = std::min(rows + 4.0f, 320.0f);
    ImGui::BeginChild("##timeline", ImVec2(width, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float innerWidth = std::max(ImGui::GetContentRegionAvail().x, 50.0f);
    ImGui::InvisibleButton("##timelineArea", ImVec2(innerWidth, rows), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // The visible range: wheel zooms around the mouse, a drag pans, a double click shows it all.
    const double whole = std::max(frame.cpuMs, 0.001);
    double span = m_ViewSpan > 0.0f ? std::min<double>(m_ViewSpan, whole) : whole;
    double start = std::clamp<double>(m_ViewStart, 0.0, whole - span);
    if (hovered)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const double mouseMs = start + (io.MousePos.x - origin.x) / innerWidth * span;
        if (io.MouseWheel != 0.0f)
        {
            const double newSpan = std::clamp(span * (io.MouseWheel > 0.0f ? 0.8 : 1.25), 0.005, whole);
            start = std::clamp(mouseMs - (mouseMs - start) * newSpan / span, 0.0, whole - newSpan);
            span = newSpan;
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            start = 0.0;
            span = whole;
        }
    }
    if (ImGui::IsItemActive() && ImGui::GetIO().MouseDelta.x != 0.0f)
        start = std::clamp(start - ImGui::GetIO().MouseDelta.x / innerWidth * span, 0.0, whole - span);
    m_ViewStart = static_cast<float>(start);
    m_ViewSpan = span >= whole ? 0.0f : static_cast<float>(span);

    const auto xOf = [&](double ms) { return origin.x + static_cast<float>((ms - start) / span) * innerWidth; };
    draw->PushClipRect(origin, ImVec2(origin.x + innerWidth, origin.y + rows), true);

    // The ruler.
    const double step = TickStep(span, innerWidth, 90.0f);
    for (double t = std::ceil(start / step) * step; t <= start + span; t += step)
    {
        const float x = xOf(t);
        draw->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + rows), IM_COL32(255, 255, 255, 25));
        char label[32];
        std::snprintf(label, sizeof(label), step < 0.1 ? "%.3f ms" : step < 1.0 ? "%.2f ms" : "%.1f ms", t);
        draw->AddText(ImVec2(x + 3.0f, origin.y + 1.0f), IM_COL32(255, 255, 255, 120), label);
    }

    const Profiler::Sample* under = nullptr;
    const ImVec2 mouse = ImGui::GetMousePos();
    for (const auto& [thread, y] : rowOf)
        draw->AddText(ImVec2(origin.x + 4.0f, origin.y + y - rowHeight + 2.0f), IM_COL32(255, 255, 255, 150), Profiler::ThreadName(thread).c_str());
    for (const Profiler::Sample& sample : frame.samples)
    {
        const float x0 = xOf(sample.start - frame.start);
        const float x1 = std::max(xOf(sample.end - frame.start), x0 + 1.0f);
        if (x1 < origin.x || x0 > origin.x + innerWidth)
            continue;
        const float y0 = origin.y + rowOf[sample.thread] + sample.depth * rowHeight;
        const ImVec2 a(x0, y0 + 1.0f), b(x1, y0 + rowHeight - 1.0f);
        draw->AddRectFilled(a, b, ColorOf(sample.name), 2.0f);
        if (x1 - x0 > 30.0f)
        {
            char label[96];
            std::snprintf(label, sizeof(label), "%s %.2f", sample.name, sample.end - sample.start);
            draw->PushClipRect(a, b, true);
            draw->AddText(ImVec2(std::max(x0, origin.x) + 3.0f, y0 + 2.0f), IM_COL32(255, 255, 255, 235), label);
            draw->PopClipRect();
        }
        if (hovered && mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y)
            under = &sample;
    }

    const Profiler::GpuPass* underGpu = nullptr;
    if (!frame.gpu.empty())
    {
        draw->AddText(ImVec2(origin.x + 4.0f, origin.y + gpuRow - rowHeight + 2.0f), IM_COL32(255, 255, 255, 150),
                      "GPU (its own clock, from the first pass)");
        for (const Profiler::GpuPass& pass : frame.gpu)
        {
            const float x0 = xOf(pass.start);
            const float x1 = std::max(xOf(pass.start + pass.ms), x0 + 1.0f);
            const ImVec2 a(x0, origin.y + gpuRow + 1.0f), b(x1, origin.y + gpuRow + rowHeight - 1.0f);
            draw->AddRectFilled(a, b, IM_COL32(215, 110, 50, 255), 2.0f);
            if (x1 - x0 > 30.0f)
            {
                draw->PushClipRect(a, b, true);
                draw->AddText(ImVec2(x0 + 3.0f, a.y + 1.0f), IM_COL32(255, 255, 255, 235), pass.name.c_str());
                draw->PopClipRect();
            }
            if (hovered && mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y)
                underGpu = &pass;
        }
    }
    draw->PopClipRect();

    if (under)
    {
        ImGui::BeginTooltip();
        ImGui::Text("%s", under->name);
        ImGui::Text("%.3f ms, from %.3f ms (%s)", under->end - under->start, under->start - frame.start, Profiler::ThreadName(under->thread).c_str());
        ImGui::EndTooltip();
    }
    else if (underGpu)
    {
        ImGui::BeginTooltip();
        ImGui::Text("%s (GPU)", underGpu->name.c_str());
        ImGui::Text("%.3f ms", underGpu->ms);
        ImGui::EndTooltip();
    }
    else if (hovered && !ImGui::IsItemActive())
    {
        ImGui::SetTooltip("Wheel zooms, drag pans, double click shows the whole frame");
    }
    ImGui::EndChild();
}

void ProfilerWindow::DrawScopes(const Profiler::Frame& frame)
{
    ImGui::Checkbox("Average of the last 120 frames", &m_Average);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Off: the selected frame only. Max is the most in one frame either way.");

    (void)frame;
    const std::vector<Profiler::ScopeStats>& stats = m_Stats;
    std::vector<std::vector<int>> children(stats.size());
    std::vector<int> roots;
    for (size_t i = 0; i < stats.size(); ++i)
        (stats[i].parent >= 0 ? children[stats[i].parent] : roots).push_back(static_cast<int>(i));
    const auto byTotal = [&](int a, int b) {
        if (stats[a].thread != stats[b].thread)
            return stats[a].thread < stats[b].thread;
        return stats[a].totalMs > stats[b].totalMs;
    };
    std::sort(roots.begin(), roots.end(), byTotal);
    for (std::vector<int>& list : children)
        std::sort(list.begin(), list.end(), byTotal);

    const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("##scopes", 5, flags))
        return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Total ms", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Self ms", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableHeadersRow();

    const std::function<void(int)> row = [&](int index) {
        const Profiler::ScopeStats& s = stats[index];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(index);
        ImGuiTreeNodeFlags node = ImGuiTreeNodeFlags_SpanFullWidth;
        if (children[index].empty())
            node |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        else if (s.depth < 2)
            node |= ImGuiTreeNodeFlags_DefaultOpen;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ColorOf(s.name) | IM_COL32(60, 60, 60, 0)));
        std::string label = s.name;
        if (s.parent < 0 && s.thread != 0)
            label += " (" + Profiler::ThreadName(s.thread) + ")";
        const bool open = ImGui::TreeNodeEx(label.c_str(), node);
        ImGui::PopStyleColor();
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", s.calls);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", s.totalMs);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", s.selfMs);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", s.maxMs);
        if (open && !children[index].empty())
        {
            for (int child : children[index])
                row(child);
            ImGui::TreePop();
        }
        ImGui::PopID();
    };
    for (int root : roots)
        row(root);
    ImGui::EndTable();
}

void ProfilerWindow::DrawGpu(const Profiler::Frame& frame)
{
    if (!Profiler::GpuTimed())
    {
        ImGui::TextWrapped("%s", Profiler::GpuStatus().c_str());
        return;
    }
    ImGui::TextDisabled("%s", Profiler::GpuStatus().c_str());
    if (frame.gpuMs < 0.0)
    {
        ImGui::TextDisabled("This frame's GPU times haven't come back.");
        return;
    }

    const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("##gpu", 4, flags))
        return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("This frame ms", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Average ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableHeadersRow();
    for (const Profiler::GpuPass& pass : frame.gpu)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(pass.name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", pass.ms);
        const auto found = m_GpuAverages.find(pass.name);
        const GpuAverage a = found != m_GpuAverages.end() ? found->second : GpuAverage{};
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", a.average);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", a.max);
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("All passes");
    ImGui::TableNextColumn();
    ImGui::Text("%.3f", frame.gpuMs);
    ImGui::EndTable();
}

void ProfilerWindow::DrawCounters(const Profiler::Frame& frame)
{
    const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("##counters", 4, flags))
        return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Counter", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("This frame", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Average", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Last 120 frames", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (const Profiler::Counter& counter : frame.counters)
    {
        const auto found = m_Counters.find(counter.name);
        if (found == m_Counters.end())
            continue;
        const CounterHistory& history = found->second;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(counter.name);
        ImGui::TableNextColumn();
        ImGui::Text("%.0f", counter.value);
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", history.average);
        ImGui::TableNextColumn();
        ImGui::PushID(counter.name);
        ImGui::PlotLines("##history", history.values.data(), static_cast<int>(history.values.size()), 0, nullptr, 0.0f, FLT_MAX,
                         ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 1.4f));
        ImGui::PopID();
    }
    ImGui::EndTable();
}
