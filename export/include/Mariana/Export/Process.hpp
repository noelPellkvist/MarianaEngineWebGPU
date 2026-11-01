#pragma once
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Running other programs (CMake, Emscripten's tools, a built game). Natively on Windows; elsewhere
// starting one fails with an error saying so.
namespace Process
{
    struct Options
    {
        std::filesystem::path workingDirectory; // empty = this process's
        // Set (or, with an empty value, removed) in the program's environment, on top of this one's.
        std::vector<std::pair<std::string, std::string>> environment;
        // Set from another thread: the program and everything it started are ended.
        const std::atomic<bool>* cancel = nullptr;
    };

    // Runs `args` (the program first) and waits for it, handing each line it prints (stdout and
    // stderr) to `line`. Its exit code, or -1 when it couldn't start or was cancelled (`error`).
    int Run(const std::vector<std::string>& args, const Options& options, const std::function<void(const std::string&)>& line, std::string& error);

    // A program left running (a local web server): ended by Stop or when this goes away, with
    // everything it started. Its output is dropped.
    class Background
    {
    public:
        Background();
        ~Background();
        Background(const Background&) = delete;
        Background& operator=(const Background&) = delete;

        bool Start(const std::vector<std::string>& args, const Options& options, std::string& error);
        void Stop();
        bool IsRunning() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };

    // Starts a program that lives on its own (a game), without waiting for it.
    bool Launch(const std::vector<std::string>& args, const std::filesystem::path& workingDirectory, std::string& error);

    // A program found on PATH (with .exe on Windows), or empty.
    std::filesystem::path FindOnPath(const std::string& name);
}
