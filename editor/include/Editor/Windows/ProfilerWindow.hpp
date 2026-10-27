#pragma once
#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Foundation/Profiler.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Where the frame time goes (the engine's Profiler): a graph of the last frames' CPU and GPU time,
// the selected frame's scopes on a timeline, and tabs with the scopes as a tree (calls, total, self,
// max), the GPU passes and the counters. Clicking a frame pauses on it; Live follows the newest.
class ProfilerWindow final : public EditorWindow
{
public:
    // `saveTrace` keeps a Chrome trace of the frames (JSON) somewhere the user can open it.
    ProfilerWindow(GUI& gui, std::function<void(const std::string& trace)> saveTrace);
    void Draw() override;

private:
    const Profiler::Frame* Selected() const;
    void DrawToolbar(const Profiler::Frame* frame);
    void DrawFrameGraph();
    void DrawTimeline(const Profiler::Frame& frame);
    void DrawScopes(const Profiler::Frame& frame);
    void DrawGpu(const Profiler::Frame& frame);
    void DrawCounters(const Profiler::Frame& frame);
    // The frames the tabs average over: the last ones up to the selected frame.
    std::vector<const Profiler::Frame*> Window(size_t count) const;
    // What the tabs show, made again when the frame shown changes (live: at most 4 times a second).
    void Refresh(const Profiler::Frame& frame);
    // A frame's wait for the display (its top-level "Present"), worked out once per frame.
    double WaitMs(const Profiler::Frame& frame);

    struct GpuAverage
    {
        double average = 0.0;
        double max = 0.0;
    };
    struct CounterHistory
    {
        std::vector<float> values;
        double average = 0.0;
    };
    std::unordered_map<uint64_t, double> m_Waits;
    std::vector<Profiler::ScopeStats> m_Stats;
    std::map<std::string, GpuAverage> m_GpuAverages;
    std::map<std::string, CounterHistory> m_Counters;
    uint64_t m_RefreshedFrame = 0;
    bool m_RefreshedAverage = true;
    double m_RefreshedAt = -1e9;

    std::function<void(const std::string&)> m_SaveTrace;
    uint64_t m_Selected = 0; // the frame shown; 0 = the newest (live)
    float m_ViewStart = 0.0f; // the timeline's visible ms range
    float m_ViewSpan = 0.0f;  // 0 = the whole frame
    bool m_Average = true;    // the tabs: the last frames, or the selected frame only
};
