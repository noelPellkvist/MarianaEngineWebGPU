#pragma once
// The profiler's bookkeeping: scopes nest and keep their order, counters add up, frames are kept up
// to kFrames, GPU times land on their frame, and a trace is JSON a trace viewer reads.
#include "TestCommon.hpp"

#include <Mariana/Foundation/Profiler.hpp>

#include <json.hpp>

#include <chrono>
#include <cstring>
#include <thread>

namespace EngineScenarios
{
    namespace detail
    {
        inline void Busy(double ms)
        {
            const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double, std::milli>(ms);
            while (std::chrono::steady_clock::now() < until)
            {
            }
        }

        inline const Profiler::Sample* Find(const Profiler::Frame& frame, const char* name)
        {
            for (const Profiler::Sample& sample : frame.samples)
                if (std::strcmp(sample.name, name) == 0)
                    return &sample;
            return nullptr;
        }
    }

    inline Result ProfilerScopes(const Log& log)
    {
        using namespace detail;
        Profiler::Clear();
        Profiler::BeginFrame();
        {
            MARIANA_PROFILE("Outer");
            {
                MARIANA_PROFILE("First");
                Busy(1.0);
            }
            {
                MARIANA_PROFILE_NAME(std::string("Sec") + "ond");
                Busy(2.0);
            }
            Busy(0.5);
        }
        std::thread worker([] {
            MARIANA_PROFILE("On a worker");
            Busy(0.5);
        });
        worker.join();
        Profiler::Count("Things", 2.0);
        Profiler::Count("Things", 3.0);
        const uint64_t index = Profiler::FrameIndex();
        Profiler::EndFrame();
        Profiler::SetGpuPasses(index, { { "Pass A", 0.0, 1.5 }, { "Pass B", 1.5, 0.25 } });

        const Profiler::Frame& frame = Profiler::Frames().back();
        const Profiler::Sample* outer = Find(frame, "Outer");
        const Profiler::Sample* first = Find(frame, "First");
        const Profiler::Sample* second = Find(frame, "Second");
        const Profiler::Sample* onWorker = Find(frame, "On a worker");
        bool passed = outer && first && second && onWorker && frame.index == index;
        if (!passed)
            return Finish(log, "profiler scopes", "a scope is missing", false);

        // Begun in order, nested by depth, inside each other.
        passed &= frame.samples[0].name == outer->name && outer->depth == 0 && first->depth == 1 && second->depth == 1;
        passed &= first->start >= outer->start && first->end <= second->start && second->end <= outer->end;
        passed &= onWorker->thread != 0 && outer->thread == 0;
        passed &= std::strcmp(Profiler::Intern("Second"), second->name) == 0 && Profiler::Intern("Second") == second->name;
        passed &= frame.counters.size() == 1 && frame.counters[0].value == 5.0;
        passed &= frame.gpuMs == 1.75 && frame.gpu.size() == 2;

        // The tree: Outer's self time is what its children didn't take.
        const std::vector<const Profiler::Frame*> frames{ &frame };
        const std::vector<Profiler::ScopeStats> stats = Profiler::Summarize(frames);
        const Profiler::ScopeStats* o = nullptr;
        double children = 0.0;
        for (const Profiler::ScopeStats& s : stats)
        {
            if (s.name == "Outer")
                o = &s;
            if (s.name == "First" || s.name == "Second")
                children += s.totalMs;
        }
        passed &= o && std::abs(o->selfMs - (o->totalMs - children)) < 1e-9 && o->selfMs > 0.3 && children > 2.9;
        return Finish(log, "profiler scopes",
                      Format("Outer %.3f ms (self %.3f), First %.3f, Second %.3f", outer->end - outer->start, o ? o->selfMs : -1.0,
                             first->end - first->start, second->end - second->start) +
                          Format(", a worker's %.3f ms on thread %.0f; a counter at 5, GPU 1.75 ms", onWorker->end - onWorker->start, onWorker->thread),
                      passed);
    }

    inline Result ProfilerFrames(const Log& log)
    {
        using namespace detail;
        Profiler::Clear();
        for (size_t i = 0; i < Profiler::kFrames + 25; ++i)
        {
            Profiler::BeginFrame();
            MARIANA_PROFILE("Frame work");
            Profiler::EndFrame();
        }
        const uint64_t last = Profiler::FrameIndex();
        const std::deque<Profiler::Frame>& frames = Profiler::Frames();
        bool passed = frames.size() == Profiler::kFrames && frames.back().index == last && frames.front().index == last - Profiler::kFrames + 1;
        // A scope still open when its frame ends closes with it.
        passed &= frames.back().samples.size() == 1 && frames.back().samples[0].end >= frames.back().samples[0].start;

        // Paused, frames run but aren't kept.
        Profiler::SetPaused(true);
        Profiler::BeginFrame();
        Profiler::EndFrame();
        Profiler::SetPaused(false);
        passed &= Profiler::Frames().back().index == last;

        // A trace a viewer reads: every scope as a complete event.
        std::vector<const Profiler::Frame*> some;
        for (size_t i = frames.size() - 3; i < frames.size(); ++i)
            some.push_back(&frames[i]);
        size_t scopes = 0;
        bool parsed = false;
        try
        {
            const nlohmann::json trace = nlohmann::json::parse(Profiler::Trace(some));
            for (const nlohmann::json& event : trace["traceEvents"])
                scopes += event["ph"] == "X" && event["name"] == "Frame work" ? 1 : 0;
            parsed = true;
        }
        catch (const std::exception&)
        {
        }
        passed &= parsed && scopes == 3;
        return Finish(log, "profiler frames", Format("kept %.0f of %.0f frames, newest %.0f; the trace parses with %.0f scopes", static_cast<double>(frames.size()),
                                                     static_cast<double>(Profiler::kFrames + 25), static_cast<double>(last), static_cast<double>(scopes)),
                      passed);
    }
}
