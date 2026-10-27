#include <Mariana/Foundation/Profiler.hpp>

#include <json.hpp>

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <tuple>
#include <thread>
#include <unordered_set>

namespace Profiler
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        struct State
        {
            Clock::time_point epoch = Clock::now();
            std::mutex mutex; // other threads' samples, names, thread numbers
            bool inFrame = false;
            uint64_t frameIndex = 0;
            Frame current;
            std::vector<Sample> others; // other threads' samples that ended since the last EndFrame
            std::deque<Frame> frames;
            bool paused = false;
            bool gpuTimed = false;
            std::string gpuStatus = "The GPU timer hasn't started.";
            std::unordered_set<std::string> names;
            uint16_t threadCount = 1; // 0 is the main thread
        };

        State& S()
        {
            static State state;
            return state;
        }

        thread_local uint16_t t_Depth = 0;
        thread_local int t_Thread = -1;

        uint16_t ThreadIndex()
        {
            if (t_Thread < 0)
            {
                State& s = S();
                std::lock_guard lock(s.mutex);
                t_Thread = s.threadCount++;
            }
            return static_cast<uint16_t>(t_Thread);
        }
    }

    double Now()
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - S().epoch).count();
    }

    void SetMainThread()
    {
        t_Thread = 0;
    }

    void BeginFrame()
    {
        State& s = S();
        t_Thread = 0;
        if (s.inFrame)
            EndFrame();
        const size_t lastSamples = s.current.samples.size();
        s.current = Frame{};
        s.current.index = ++s.frameIndex;
        s.current.start = Now();
        s.current.samples.reserve(lastSamples + 16);
        s.inFrame = true;
    }

    void EndFrame()
    {
        State& s = S();
        if (!s.inFrame)
            return;
        const double end = Now();
        s.current.cpuMs = end - s.current.start;
        // Scopes still open (around the loop, say) end with the frame.
        for (Sample& sample : s.current.samples)
            if (sample.end < sample.start)
                sample.end = end;
        {
            std::lock_guard lock(s.mutex);
            s.current.samples.insert(s.current.samples.end(), s.others.begin(), s.others.end());
            s.others.clear();
        }
        s.inFrame = false;
        if (s.paused)
            return;
        s.frames.push_back(std::move(s.current));
        s.current = Frame{};
        while (s.frames.size() > kFrames)
            s.frames.pop_front();
    }

    uint64_t FrameIndex()
    {
        return S().frameIndex;
    }

    const char* Intern(std::string_view name)
    {
        State& s = S();
        std::lock_guard lock(s.mutex);
        return s.names.emplace(name).first->c_str();
    }

    void Count(const char* name, double value)
    {
        State& s = S();
        if (!s.inFrame)
            return;
        for (Counter& counter : s.current.counters)
        {
            if (counter.name == name || std::string_view(counter.name) == name)
            {
                counter.value += value;
                return;
            }
        }
        s.current.counters.push_back({ name, value });
    }

    void SetGpuPasses(uint64_t frame, std::vector<GpuPass> passes)
    {
        State& s = S();
        for (auto it = s.frames.rbegin(); it != s.frames.rend(); ++it)
        {
            if (it->index < frame)
                return;
            if (it->index != frame)
                continue;
            double total = 0.0;
            for (const GpuPass& pass : passes)
                total += pass.ms;
            it->gpu = std::move(passes);
            it->gpuMs = total;
            return;
        }
    }

    void SetGpuStatus(bool timed, std::string status)
    {
        S().gpuTimed = timed;
        S().gpuStatus = std::move(status);
    }

    bool GpuTimed()
    {
        return S().gpuTimed;
    }

    const std::string& GpuStatus()
    {
        return S().gpuStatus;
    }

    void SetPaused(bool paused)
    {
        S().paused = paused;
    }

    bool Paused()
    {
        return S().paused;
    }

    void Clear()
    {
        S().frames.clear();
    }

    const std::deque<Frame>& Frames()
    {
        return S().frames;
    }

    std::string ThreadName(uint16_t thread)
    {
        return thread == 0 ? std::string("Main") : "Worker " + std::to_string(thread);
    }

    std::string Trace(const std::vector<const Frame*>& frames)
    {
        using Json = nlohmann::ordered_json;
        constexpr int kFramesTrack = 1000, kGpuTrack = 1001;
        Json events = Json::array();
        const auto track = [&](int tid, const std::string& name) {
            events.push_back({ { "name", "thread_name" }, { "ph", "M" }, { "pid", 1 }, { "tid", tid }, { "args", { { "name", name } } } });
        };
        track(kFramesTrack, "Frames");
        track(kGpuTrack, "GPU");
        std::unordered_set<uint16_t> threads;

        for (const Frame* frame : frames)
        {
            events.push_back({ { "name", "Frame " + std::to_string(frame->index) }, { "cat", "frame" }, { "ph", "X" },
                               { "ts", frame->start * 1000.0 }, { "dur", frame->cpuMs * 1000.0 }, { "pid", 1 }, { "tid", kFramesTrack } });
            for (const Sample& sample : frame->samples)
            {
                threads.insert(sample.thread);
                events.push_back({ { "name", sample.name }, { "cat", "cpu" }, { "ph", "X" }, { "ts", sample.start * 1000.0 },
                                   { "dur", (sample.end - sample.start) * 1000.0 }, { "pid", 1 }, { "tid", sample.thread } });
            }
            // The GPU's clock isn't the CPU's: its passes are put from the frame's start on.
            for (const GpuPass& pass : frame->gpu)
                events.push_back({ { "name", pass.name }, { "cat", "gpu" }, { "ph", "X" }, { "ts", (frame->start + pass.start) * 1000.0 },
                                   { "dur", pass.ms * 1000.0 }, { "pid", 1 }, { "tid", kGpuTrack } });
            for (const Counter& counter : frame->counters)
                events.push_back({ { "name", counter.name }, { "ph", "C" }, { "ts", frame->start * 1000.0 }, { "pid", 1 },
                                   { "args", { { "value", counter.value } } } });
        }
        for (uint16_t thread : threads)
            track(thread, ThreadName(thread));
        return Json{ { "displayTimeUnit", "ms" }, { "traceEvents", events } }.dump();
    }

    std::vector<ScopeStats> Summarize(const std::vector<const Frame*>& frames, bool byName)
    {
        std::vector<ScopeStats> stats;
        // An entry by (the entry around it, name, thread); by name, the parent is always -1.
        std::map<std::tuple<int, std::string_view, uint16_t>, int> index;
        std::vector<double> frameTotal; // per entry, in the frame being walked
        struct Open
        {
            double end;
            int entry;
        };

        for (const Frame* frame : frames)
        {
            std::fill(frameTotal.begin(), frameTotal.end(), 0.0);
            // Per thread, by when they began (the main thread's are already in that order).
            std::vector<size_t> order(frame->samples.size());
            for (size_t i = 0; i < order.size(); ++i)
                order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                const Sample& x = frame->samples[a];
                const Sample& y = frame->samples[b];
                return x.thread != y.thread ? x.thread < y.thread : x.start < y.start;
            });

            std::vector<Open> open;
            int thread = -1;
            for (size_t sampleIndex : order)
            {
                const Sample& sample = frame->samples[sampleIndex];
                if (sample.thread != thread)
                {
                    open.clear();
                    thread = sample.thread;
                }
                while (!open.empty() && open.back().end <= sample.start)
                    open.pop_back();
                const double ms = sample.end - sample.start;
                const int around = open.empty() ? -1 : open.back().entry;
                // The scope around it spent this much in it rather than in itself.
                if (around >= 0)
                    stats[around].selfMs -= ms;

                const int parent = byName ? -1 : around;
                auto [it, added] = index.emplace(std::make_tuple(parent, std::string_view(sample.name), sample.thread), static_cast<int>(stats.size()));
                if (added)
                {
                    ScopeStats entry;
                    entry.name = sample.name;
                    entry.parent = parent;
                    entry.depth = static_cast<uint16_t>(parent >= 0 ? stats[parent].depth + 1 : 0);
                    entry.thread = sample.thread;
                    stats.push_back(std::move(entry));
                    frameTotal.push_back(0.0);
                }
                const int entry = it->second;
                stats[entry].calls += 1.0;
                stats[entry].totalMs += ms;
                stats[entry].selfMs += ms;
                frameTotal[entry] += ms;
                open.push_back({ sample.end, entry });
            }
            for (size_t e = 0; e < stats.size(); ++e)
                stats[e].maxMs = std::max(stats[e].maxMs, frameTotal[e]);
        }

        const double count = frames.empty() ? 1.0 : static_cast<double>(frames.size());
        for (ScopeStats& entry : stats)
        {
            entry.calls /= count;
            entry.totalMs /= count;
            entry.selfMs /= count;
        }
        return stats;
    }

    Scope::Scope(const char* name)
        : m_Name(name), m_Start(Now()), m_Frame(0), m_Index(~size_t(0)), m_Thread(ThreadIndex()), m_Depth(t_Depth++)
    {
        if (m_Thread != 0)
            return;
        State& s = S();
        if (!s.inFrame)
            return;
        m_Frame = s.frameIndex;
        m_Index = s.current.samples.size();
        s.current.samples.push_back({ name, m_Start, -1.0, m_Depth, 0 });
    }

    Scope::~Scope()
    {
        --t_Depth;
        const double end = Now();
        State& s = S();
        if (m_Thread == 0)
        {
            if (m_Index != ~size_t(0) && s.inFrame && s.frameIndex == m_Frame)
                s.current.samples[m_Index].end = end;
            return;
        }
        std::lock_guard lock(s.mutex);
        s.others.push_back({ m_Name, m_Start, end, m_Depth, m_Thread });
    }
}
