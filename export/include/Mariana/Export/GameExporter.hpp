#pragma once
#include <Mariana/Export/BuildProfile.hpp>
#include <Mariana/Export/GameFiles.hpp>
#include <Mariana/Export/Process.hpp>

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Where the exporter finds what it builds with. The defaults are where this program was built from:
// the engine's source and its CMake, the build folder it was built in (natively), build-web-release
// / build-web next to the source (the web), and the Emscripten SDK in EMSDK. A project can keep its
// own in .mariana/build-tools.json (this machine's paths, not the project's).
struct BuildTools
{
    std::filesystem::path source;
    std::filesystem::path cmake;
    std::filesystem::path nativeBuild;
    std::filesystem::path webBuildRelease;
    std::filesystem::path webBuildDebug;
    std::filesystem::path emsdk;

    static BuildTools Defaults();
    static BuildTools Load(const std::filesystem::path& project);
    bool Save(const std::filesystem::path& project, std::string& error) const;

    std::filesystem::path WebBuild(BuildConfiguration configuration) const;
    std::filesystem::path Emscripten() const; // <emsdk>/upstream/emscripten
    std::filesystem::path Python() const;     // the SDK's own
    // What is missing to build for `platform` (empty when nothing is).
    std::string Missing(BuildPlatform platform) const;
    // What Emscripten's tools need in their environment.
    std::vector<std::pair<std::string, std::string>> WebEnvironment() const;
};

// Builds a game from a project with a profile: checks the profile and the tools, builds the player
// (CMake, incremental), lays out the output folder and checks the result.
//   Windows: <output>/<gameName>.exe (the player, no DLLs), data/engine (the engine's resources)
//            and data/game (the project's files the game uses, and its Player.json); the exe
//            finds data/ next to itself. Checked by running it with --check.
//   Web:     the web runtime (index.html/.js/.wasm/.data: the player with the engine's resources)
//            and game.data + game.data.js (the game's files, mounted at /game), a static site to
//            serve over http(s).
// Start runs it on a thread of its own (the editor), Run on the caller's (the command line tool).
class GameExporter
{
public:
    struct Line
    {
        std::string text;
        bool error = false;
    };

    GameExporter(std::filesystem::path project, BuildProfile profile, BuildTools tools);
    ~GameExporter();
    GameExporter(const GameExporter&) = delete;
    GameExporter& operator=(const GameExporter&) = delete;

    void Start();
    bool Run();
    void Cancel();
    bool IsRunning() const { return m_Running; }
    bool IsDone() const { return m_Done; }
    bool Succeeded() const { return m_Succeeded; }
    // The step under way and how far along the steps are (0..1).
    std::string Step() const;
    float Progress() const { return m_Progress; }
    // The lines since the last call (the editor shows them); with `onLine` set, each also goes there
    // as it comes (the command line tool prints them).
    std::vector<Line> TakeLines();
    std::function<void(const Line&)> onLine;

    const BuildProfile& Profile() const { return m_Profile; }
    std::filesystem::path Output() const { return m_Profile.OutputFolder(m_Project); }
    // What starts the game: the exe, or the web build's index.html.
    std::filesystem::path Launchable() const;

private:
    std::filesystem::path m_Project;
    BuildProfile m_Profile;
    BuildTools m_Tools;
    std::thread m_Thread;
    std::atomic<bool> m_Cancel = false;
    std::atomic<bool> m_Running = false;
    std::atomic<bool> m_Done = false;
    std::atomic<bool> m_Succeeded = false;
    std::atomic<float> m_Progress = 0.0f;
    mutable std::mutex m_Mutex;
    std::string m_Step;
    std::vector<Line> m_Lines;

    bool Export();
    bool ExportWindows(const GameFiles& files);
    bool ExportWeb(const GameFiles& files);
    void SetStep(const std::string& step, float progress);
    void Log(const std::string& text, bool error = false);
    // Runs a tool, its lines into the log; false (logged) unless it exits with 0.
    bool Tool(const std::vector<std::string>& args, const Process::Options& options);
};

// Starts a built game: the exe on its own, or the web build served on localhost (Emscripten's emrun,
// which opens the browser) until Stop.
class GameRunner
{
public:
    bool Start(const BuildProfile& profile, const std::filesystem::path& output, const BuildTools& tools, std::string& error);
    void Stop() { m_Server.Stop(); }
    bool IsServing() const { return m_Server.IsRunning(); }
    const std::string& Address() const { return m_Address; }

private:
    Process::Background m_Server;
    std::string m_Address;
};
