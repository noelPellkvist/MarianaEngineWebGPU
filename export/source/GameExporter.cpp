#include <Mariana/Export/GameExporter.hpp>

#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;

namespace
{
    bool Exists(const fs::path& path)
    {
        std::error_code ec;
        return !path.empty() && fs::exists(path, ec);
    }

    // A file name made of a game's name: what Windows doesn't allow in one becomes '_'.
    std::string SafeName(const std::string& name)
    {
        std::string out;
        for (char c : name)
            out += std::string("<>:\"/\\|?*").find(c) != std::string::npos || static_cast<unsigned char>(c) < 32 ? '_' : c;
        while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
            out.pop_back();
        return out.empty() ? std::string("Game") : out;
    }

    // A line of a tool's output that reports a failure (shown red).
    bool IsErrorLine(const std::string& line)
    {
        static const char* const markers[] = { ": error", "error C", "error LNK", "CMake Error", "FAILED:", "Error:", "error:" };
        return std::any_of(std::begin(markers), std::end(markers), [&](const char* m) { return line.find(m) != std::string::npos; });
    }

    uintmax_t FolderSize(const fs::path& folder)
    {
        std::error_code ec;
        uintmax_t size = 0;
        for (auto it = fs::recursive_directory_iterator(folder, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
            if (it->is_regular_file(ec))
                size += it->file_size(ec);
        return size;
    }

    std::string Megabytes(uintmax_t bytes)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        return text;
    }

    bool CopyFolder(const fs::path& from, const fs::path& to, std::string& error)
    {
        std::error_code ec;
        fs::create_directories(to, ec);
        fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec)
            error = "Could not copy " + from.string() + " to " + to.string() + ": " + ec.message();
        return !ec;
    }

    std::string PathString(const fs::path& path) { return path.generic_string(); }
}

BuildTools BuildTools::Defaults()
{
    BuildTools tools;
    tools.source = MARIANA_SOURCE_DIR;
    tools.cmake = MARIANA_CMAKE_COMMAND;
    tools.nativeBuild = MARIANA_NATIVE_BUILD_DIR;
    tools.webBuildRelease = tools.source / "build-web-release";
    tools.webBuildDebug = tools.source / "build-web";
    if (const char* emsdk = std::getenv("EMSDK"); emsdk && *emsdk)
        tools.emsdk = emsdk;
    return tools;
}

BuildTools BuildTools::Load(const fs::path& project)
{
    BuildTools tools = Defaults();
    std::ifstream stream(project / ".mariana" / "build-tools.json", std::ios::binary);
    if (!stream)
        return tools;
    std::ostringstream text;
    text << stream.rdbuf();
    const Json json = Json::parse(text.str(), nullptr, false);
    if (!json.is_object())
        return tools;
    auto read = [&](const char* key, fs::path& value) {
        if (const std::string path = json.value(key, std::string()); !path.empty())
            value = path;
    };
    read("cmake", tools.cmake);
    read("nativeBuild", tools.nativeBuild);
    read("webBuildRelease", tools.webBuildRelease);
    read("webBuildDebug", tools.webBuildDebug);
    read("emsdk", tools.emsdk);
    return tools;
}

bool BuildTools::Save(const fs::path& project, std::string& error) const
{
    const fs::path file = project / ".mariana" / "build-tools.json";
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    Json json = { { "cmake", PathString(cmake) },
                  { "nativeBuild", PathString(nativeBuild) },
                  { "webBuildRelease", PathString(webBuildRelease) },
                  { "webBuildDebug", PathString(webBuildDebug) },
                  { "emsdk", PathString(emsdk) } };
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << json.dump(2) << "\n";
    if (!stream)
        error = "Could not write " + file.string();
    return static_cast<bool>(stream);
}

fs::path BuildTools::WebBuild(BuildConfiguration configuration) const
{
    return configuration == BuildConfiguration::Debug ? webBuildDebug : webBuildRelease;
}

fs::path BuildTools::Emscripten() const
{
    return emsdk.empty() ? fs::path() : emsdk / "upstream" / "emscripten";
}

fs::path BuildTools::Python() const
{
    if (emsdk.empty())
        return {};
    // The one the SDK's config names, else any it has.
    std::ifstream config(emsdk / ".emscripten");
    for (std::string line; std::getline(config, line);)
    {
        const size_t at = line.find("/python/");
        if (line.rfind("PYTHON", 0) != 0 || at == std::string::npos)
            continue;
        const size_t end = line.find('\'', at);
        const fs::path python = emsdk / line.substr(at + 1, end == std::string::npos ? std::string::npos : end - at - 1);
        if (Exists(python))
            return python;
    }
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(emsdk / "python", ec))
        if (Exists(entry.path() / "python.exe"))
            return entry.path() / "python.exe";
    return Process::FindOnPath("python");
}

std::vector<std::pair<std::string, std::string>> BuildTools::WebEnvironment() const
{
    std::string path = PathString(emsdk) + ";" + PathString(Emscripten()) + ";" + PathString(Python().parent_path());
    if (const char* existing = std::getenv("PATH"))
        path += std::string(";") + existing;
    return { { "EMSDK", PathString(emsdk) }, { "EM_CONFIG", PathString(emsdk / ".emscripten") }, { "EMSDK_PYTHON", PathString(Python()) }, { "PATH", path } };
}

std::string BuildTools::Missing(BuildPlatform platform) const
{
    if (!Exists(source / "CMakeLists.txt"))
        return "No engine source at " + source.string();
    if (!Exists(cmake))
        return "No CMake at " + cmake.string();
    if (platform == BuildPlatform::Web)
    {
        if (emsdk.empty())
            return "No Emscripten SDK: set EMSDK, or its folder in the build tools";
        if (!Exists(Emscripten() / "emcc.py") || !Exists(Emscripten() / "tools" / "file_packager.py"))
            return "No Emscripten in " + emsdk.string() + " (upstream/emscripten)";
        if (Python().empty())
            return "No Python in the Emscripten SDK at " + emsdk.string();
    }
    return {};
}

GameExporter::GameExporter(fs::path project, BuildProfile profile, BuildTools tools)
    : m_Project(std::move(project)), m_Profile(std::move(profile)), m_Tools(std::move(tools))
{
}

GameExporter::~GameExporter()
{
    Cancel();
    if (m_Thread.joinable())
        m_Thread.join();
}

void GameExporter::Start()
{
    if (m_Running || m_Thread.joinable())
        return;
    m_Running = true;
    m_Thread = std::thread([this]() { Run(); });
}

bool GameExporter::Run()
{
    m_Running = true;
    m_Succeeded = Export() && !m_Cancel;
    if (m_Cancel)
        Log("Cancelled", true);
    SetStep(m_Succeeded ? "Done" : m_Cancel ? "Cancelled" : "Failed", m_Succeeded ? 1.0f : m_Progress.load());
    m_Done = true;
    m_Running = false;
    return m_Succeeded;
}

void GameExporter::Cancel()
{
    m_Cancel = true;
}

std::string GameExporter::Step() const
{
    std::lock_guard lock(m_Mutex);
    return m_Step;
}

std::vector<GameExporter::Line> GameExporter::TakeLines()
{
    std::lock_guard lock(m_Mutex);
    return std::exchange(m_Lines, {});
}

fs::path GameExporter::Launchable() const
{
    return m_Profile.platform == BuildPlatform::Web ? Output() / "index.html" : Output() / (SafeName(m_Profile.gameName) + ".exe");
}

void GameExporter::SetStep(const std::string& step, float progress)
{
    {
        std::lock_guard lock(m_Mutex);
        m_Step = step;
    }
    m_Progress = progress;
    if (step != "Done" && step != "Failed" && step != "Cancelled")
        Log("== " + step);
}

void GameExporter::Log(const std::string& text, bool error)
{
    const Line line{ text, error };
    if (onLine)
        onLine(line);
    std::lock_guard lock(m_Mutex);
    m_Lines.push_back(line);
}

bool GameExporter::Tool(const std::vector<std::string>& args, const Process::Options& given)
{
    if (m_Cancel)
        return false;
    std::string command;
    for (const std::string& arg : args)
        command += (command.empty() ? "" : " ") + (arg.find(' ') != std::string::npos ? "\"" + arg + "\"" : arg);
    Log("> " + command);
    Process::Options options = given;
    options.cancel = &m_Cancel;
    std::string error;
    const int code = Process::Run(args, options, [this](const std::string& line) { Log(line, IsErrorLine(line)); }, error);
    if (code == 0)
        return true;
    if (!m_Cancel)
        Log(error.empty() ? fs::path(args[0]).filename().string() + " failed (exit code " + std::to_string(code) + ")" : error, true);
    return false;
}

bool GameExporter::Export()
{
    SetStep("Checking the profile", 0.02f);
    Log(std::string("Building ") + m_Profile.name + " (" + BuildProfile::PlatformName(m_Profile.platform) + ", " +
        BuildProfile::ConfigurationName(m_Profile.configuration) + ") of " + m_Project.string() + " into " + Output().string());
    if (const std::string missing = m_Tools.Missing(m_Profile.platform); !missing.empty())
    {
        Log(missing, true);
        return false;
    }
    GameFiles files;
    std::string error;
    if (!GameExport::CollectFiles(m_Project, m_Profile, files, error))
    {
        Log(error, true);
        return false;
    }
    for (const std::string& warning : files.warnings)
        Log("Warning: " + warning);
    Log("The game takes " + std::to_string(files.files.size()) + " files and folders from the project");
    return m_Profile.platform == BuildPlatform::Web ? ExportWeb(files) : ExportWindows(files);
}

bool GameExporter::ExportWindows(const GameFiles& files)
{
    const char* configuration = BuildProfile::ConfigurationName(m_Profile.configuration);
    const fs::path build = m_Tools.nativeBuild;
    const std::string cmake = PathString(m_Tools.cmake);
    // MSBuild's helper processes would otherwise outlive the build.
    Process::Options options;
    options.environment = { { "MSBUILDDISABLENODEREUSE", "1" } };

    SetStep("Building the player", 0.1f);
    if (!Exists(build / "CMakeCache.txt") && !Tool({ cmake, "-S", PathString(m_Tools.source), "-B", PathString(build) }, options))
        return false;
    if (!Tool({ cmake, "--build", PathString(build), "--config", configuration, "--target", "MarianaPlayer", "--parallel" }, options))
        return false;
    fs::path player = build / "bin" / configuration / "MarianaPlayer.exe";
    if (!Exists(player))
        player = build / "bin" / "MarianaPlayer.exe";
    if (!Exists(player))
    {
        Log("The build made no MarianaPlayer.exe in " + (build / "bin").string(), true);
        return false;
    }
    if (m_Cancel)
        return false;

    SetStep("Copying the game", 0.75f);
    const fs::path out = Output();
    const fs::path exe = Launchable();
    std::error_code ec;
    fs::create_directories(out, ec);
    fs::remove_all(out / "data", ec);
    std::string error;
    if (!fs::copy_file(player, exe, fs::copy_options::overwrite_existing, ec))
    {
        Log("Could not write " + exe.string() + ": " + ec.message() + (ec ? " (is the game still running?)" : ""), true);
        return false;
    }
    if (!CopyFolder(m_Tools.source / "engine" / "resources", out / "data" / "engine", error) ||
        !GameExport::StageFiles(m_Project, files, GameExport::PlayerConfig(m_Project, m_Profile), out / "data" / "game", error))
    {
        Log(error, true);
        return false;
    }

    SetStep("Checking the game", 0.9f);
    Process::Options check;
    check.workingDirectory = out;
    if (!Tool({ PathString(exe), "--check" }, check))
        return false;
    Log("Built " + exe.string() + " (" + Megabytes(FolderSize(out)) + " with data/)");
    return true;
}

bool GameExporter::ExportWeb(const GameFiles& files)
{
    const char* configuration = BuildProfile::ConfigurationName(m_Profile.configuration);
    const fs::path build = m_Tools.WebBuild(m_Profile.configuration);
    const std::string cmake = PathString(m_Tools.cmake);
    const std::string python = PathString(m_Tools.Python());
    Process::Options options;
    options.environment = m_Tools.WebEnvironment();

    if (!Exists(build / "CMakeCache.txt"))
    {
        SetStep("Setting up the web build (once; the first build takes a while)", 0.05f);
        if (!Tool({ python, PathString(m_Tools.Emscripten() / "emcmake.py"), cmake, "-S", PathString(m_Tools.source), "-B", PathString(build), "-G", "Ninja",
                    std::string("-DCMAKE_BUILD_TYPE=") + configuration },
                  options))
            return false;
    }
    SetStep("Building the web player", 0.1f);
    if (!Tool({ cmake, "--build", PathString(build), "--target", "MarianaPlayerRuntime", "--parallel" }, options))
        return false;
    const fs::path runtime = build / "player-runtime";
    const char* const runtimeFiles[] = { "index.html", "index.js", "index.wasm", "index.data" };
    for (const char* file : runtimeFiles)
    {
        if (!Exists(runtime / file))
        {
            Log("The build made no " + (runtime / file).string(), true);
            return false;
        }
    }
    if (m_Cancel)
        return false;

    SetStep("Copying the game", 0.75f);
    const fs::path out = Output();
    std::error_code ec;
    fs::create_directories(out, ec);
    for (const char* file : runtimeFiles)
    {
        if (!fs::copy_file(runtime / file, out / file, fs::copy_options::overwrite_existing, ec))
        {
            Log("Could not write " + (out / file).string() + ": " + ec.message(), true);
            return false;
        }
    }
    std::string error;
    const fs::path staging = build / "export" / SafeName(m_Profile.name) / "game";
    if (!GameExport::StageFiles(m_Project, files, GameExport::PlayerConfig(m_Project, m_Profile), staging, error))
    {
        Log(error, true);
        return false;
    }

    SetStep("Packing the game's files", 0.85f);
    Process::Options pack = options;
    pack.workingDirectory = out;
    if (!Tool({ python, PathString(m_Tools.Emscripten() / "tools" / "file_packager.py"), "game.data", "--preload", PathString(staging) + "@/game",
                "--js-output=game.data.js", "--no-node", "--quiet" },
              pack))
        return false;
    Log("Built " + (out / "index.html").string() + " (" + Megabytes(FolderSize(out)) + "); serve the folder over http(s) to play it");
    return true;
}

bool GameRunner::Start(const BuildProfile& profile, const fs::path& output, const BuildTools& tools, std::string& error)
{
    if (profile.platform == BuildPlatform::Windows)
        return Process::Launch({ PathString(output / (SafeName(profile.gameName) + ".exe")) }, output, error);

    // The web build on localhost (WebGPU needs a secure context, which localhost is); emrun opens
    // the browser.
    const int port = 6931;
    Process::Options options;
    options.environment = tools.WebEnvironment();
    options.workingDirectory = output;
    if (!m_Server.Start({ PathString(tools.Python()), PathString(tools.Emscripten() / "emrun.py"), "--no_emrun_detect", "--port", std::to_string(port),
                          PathString(output / "index.html") },
                        options, error))
        return false;
    m_Address = "http://localhost:" + std::to_string(port) + "/index.html";
    return true;
}
