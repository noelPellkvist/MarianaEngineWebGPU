#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Where the time of a frame goes. Code marks what it does with scopes (MARIANA_PROFILE("Physics")),
// which record when they start and end; a frame keeps its scopes, its counters (Count) and, a few
// frames later, how long each GPU pass took. The last kFrames frames are kept for tools (the
// editor's Profiler window, traces). Always on: a scope costs two clock reads.
//
// The app's loop calls BeginFrame and EndFrame (Application does). Scopes on the main thread outside
// a frame are dropped; other threads' scopes go to the frame running when they end.
namespace Profiler
{
    struct Sample
    {
        const char* name = ""; // a literal or Intern()ed: lives as long as the program
        double start = 0.0;    // ms since the profiler started
        double end = 0.0;
        uint16_t depth = 0;    // scopes open around it on its thread
        uint16_t thread = 0;   // 0 = the main thread (the one that calls BeginFrame)
    };

    struct Counter
    {
        const char* name = "";
        double value = 0.0;
    };

    // A GPU pass's time, measured by the GPU (timestamp queries).
    struct GpuPass
    {
        std::string name;
        double start = 0.0; // ms since the frame's first pass began on the GPU
        double ms = 0.0;
    };

    struct Frame
    {
        uint64_t index = 0;
        double start = 0.0; // ms since the profiler started
        double cpuMs = 0.0; // BeginFrame to EndFrame
        double gpuMs = -1.0; // its passes added up; < 0 until they arrive (or when the GPU can't time)
        std::vector<Sample> samples; // the main thread's in the order they began, then the others'
        std::vector<Counter> counters;
        std::vector<GpuPass> gpu;
    };

    constexpr size_t kFrames = 600;

    // The calling thread is the main thread (Application's constructor; BeginFrame says so too).
    void SetMainThread();
    void BeginFrame();
    void EndFrame();
    // The frame being recorded (counts up from 1).
    uint64_t FrameIndex();
    // ms since the profiler started.
    double Now();

    // A name that lives as long as the program, the same pointer for the same text.
    const char* Intern(std::string_view name);

    // Adds `value` to this frame's counter of that name.
    void Count(const char* name, double value);

    // How long each pass of frame `frame` took on the GPU (the GPU timer, when the readback lands).
    void SetGpuPasses(uint64_t frame, std::vector<GpuPass> passes);
    // Whether the GPU is timed, or why not (shown by tools).
    void SetGpuStatus(bool timed, std::string status);
    bool GpuTimed();
    const std::string& GpuStatus();

    // Paused, frames are still run but not kept (what a tool shows stays put).
    void SetPaused(bool paused);
    bool Paused();
    void Clear();
    // The kept frames, oldest first. Main thread only.
    const std::deque<Frame>& Frames();
    // The thread a sample's `thread` names ("Main", "Worker 1", ...).
    std::string ThreadName(uint16_t thread);

    // The frames as a Chrome trace (JSON: open it in Perfetto or chrome://tracing): scopes per
    // thread, GPU passes on a track of their own, counters.
    std::string Trace(const std::vector<const Frame*>& frames);

    // A scope over some frames: per frame on average, and the most in one frame.
    struct ScopeStats
    {
        std::string name;
        int parent = -1;    // the entry of the scope around it (a tree; -1 = none, or by name)
        uint16_t depth = 0;
        uint16_t thread = 0;
        double calls = 0.0;
        double totalMs = 0.0; // with the scopes inside it
        double selfMs = 0.0;  // without them
        double maxMs = 0.0;   // total, the most in one frame
    };
    // As a tree (an entry per scope below the same scopes, parents before children, in the order
    // they first ran) or, with `byName`, an entry per name and thread wherever it ran.
    std::vector<ScopeStats> Summarize(const std::vector<const Frame*>& frames, bool byName = false);

    class Scope
    {
    public:
        explicit Scope(const char* name);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        const char* m_Name;
        double m_Start;
        uint64_t m_Frame;
        size_t m_Index; // main thread: its sample in the frame; ~0 = not recorded
        uint16_t m_Thread;
        uint16_t m_Depth;
    };
}

#define MARIANA_PROFILE_JOIN2(a, b) a##b
#define MARIANA_PROFILE_JOIN(a, b) MARIANA_PROFILE_JOIN2(a, b)
// Times the rest of the enclosing block under `name` (a string literal).
#define MARIANA_PROFILE(name) ::Profiler::Scope MARIANA_PROFILE_JOIN(marianaProfile, __LINE__)(name)
// The same with a name made at run time (interned on every use: keep it out of tight loops, or
// Intern once and use MARIANA_PROFILE with the pointer).
#define MARIANA_PROFILE_NAME(text) ::Profiler::Scope MARIANA_PROFILE_JOIN(marianaProfile, __LINE__)(::Profiler::Intern(text))
