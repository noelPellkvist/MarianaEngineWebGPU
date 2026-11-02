// MarianaExportTest: what the Sandbox's build profiles package and write, with known answers: the
// Windows profile takes Playground.scene and what it uses (CarConcept's folder, Rumba.glb, the
// dancers' graph, the scripts) and nothing it doesn't (Sponza, the editor's caches); its Player.json
// carries the profile's window and scene over the project's; profiles survive a save and load;
// staging copies the files. A console program: no window, no GPU, nothing is built. Exits 1 when a
// check fails.
#include <Mariana/Export/BuildProfile.hpp>
#include <Mariana/Export/GameFiles.hpp>
#include <Mariana/Export/Project.hpp>

#include <json.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <crtdbg.h>
#endif

namespace fs = std::filesystem;

namespace
{
    int failed = 0;
    int checks = 0;

    void Check(bool passed, const std::string& name, const std::string& detail)
    {
        ++checks;
        failed += passed ? 0 : 1;
        std::printf("  %s  %s: %s\n", passed ? "ok  " : "FAIL", name.c_str(), detail.c_str());
        std::fflush(stdout);
    }

    bool Has(const std::vector<fs::path>& files, const fs::path& file)
    {
        return std::find(files.begin(), files.end(), file) != files.end();
    }

    bool Under(const std::vector<fs::path>& files, const std::string& prefix)
    {
        return std::any_of(files.begin(), files.end(), [&](const fs::path& f) { return f.generic_string().rfind(prefix, 0) == 0; });
    }
}

int main()
{
#ifdef _WIN32
    for (int type : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
    {
        _CrtSetReportMode(type, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
    }
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    std::printf("MarianaExportTest\n");
    const fs::path sandbox = MARIANA_SANDBOX_DIR;

    BuildProfiles profiles;
    std::string error;
    const bool loaded = BuildProfiles::Load(sandbox, profiles, error);
    BuildProfile* windows = profiles.Find("Windows");
    BuildProfile* web = profiles.Find("Web");
    Check(loaded && windows && web && windows->platform == BuildPlatform::Windows && web->platform == BuildPlatform::Web, "the Sandbox's profiles",
          std::to_string(profiles.profiles.size()) + " profiles, active " + profiles.active + (error.empty() ? "" : " | " + error));
    if (!windows || !web)
        return 1;

    GameFiles files;
    const bool collected = GameExport::CollectFiles(sandbox, *windows, files, error);
    std::string listed;
    for (const fs::path& file : files.files)
        listed += (listed.empty() ? "" : ", ") + file.generic_string();
    Check(collected && Has(files.files, "Scenes/Playground.scene") && Has(files.files, "Models/CarConcept") && Has(files.files, "Models/Rumba.glb") &&
              Has(files.files, "Animations/Dancer.animgraph") && Has(files.files, "Scripts") && !Under(files.files, "Models/Sponza") &&
              !Under(files.files, ".mariana") && !Has(files.files, "Scripts/VehicleDriver.luau"),
          "what Playground takes", listed);
    for (const std::string& warning : files.warnings)
        std::printf("        warning: %s\n", warning.c_str());

    BuildProfile custom = *windows;
    custom.title = "Test Title";
    custom.width = 800;
    custom.height = 600;
    custom.logProfile = true;
    const nlohmann::json config = nlohmann::json::parse(GameExport::PlayerConfig(sandbox, custom));
    Check(config["window"]["title"] == "Test Title" && config["window"]["width"] == 800 && config["scene"] == windows->scene && config["profile"] == true &&
              config["camera"]["follow"] == windows->followCamera && config["camera"].contains("position"),
          "the game's Player.json", config.dump());

    const std::string json = profiles.ToJson();
    std::string problem;
    const BuildProfiles again = BuildProfiles::FromJson(json, problem);
    Check(problem.empty() && again.ToJson() == json && again.profiles.size() == profiles.profiles.size(), "profiles saved and read back",
          std::to_string(json.size()) + " bytes" + (problem.empty() ? "" : " | " + problem));

    // A small profile staged: only a scene that uses builtins, and the scripts.
    BuildProfile small = *windows;
    small.scene = "/Scenes/Playground.scene";
    GameFiles few;
    few.files = { "Scenes/Playground.scene", "Scripts" };
    const fs::path staging = fs::temp_directory_path() / "MarianaExportTest" / "game";
    const bool staged = GameExport::StageFiles(sandbox, few, GameExport::PlayerConfig(sandbox, small), staging, error);
    std::error_code ec;
    Check(staged && fs::exists(staging / "Player.json", ec) && fs::exists(staging / "Scenes" / "Playground.scene", ec) &&
              fs::exists(staging / "Scripts" / "VehicleDriver.luau", ec) && !fs::exists(staging / "Models", ec),
          "staging copies the files", staging.string() + (error.empty() ? "" : " | " + error));
    fs::remove_all(staging.parent_path(), ec);

    // A new project: its folders, the starter scripts and the .gitignore; a folder with files in it
    // is refused; its Defaults profiles have no start scene until it has one.
    const fs::path temp = fs::temp_directory_path() / "MarianaExportTest";
    const fs::path project = temp / "My Game";
    fs::remove_all(temp, ec);
    std::string createError, refusedError;
    const bool created = Project::Create(project, sandbox / "Scripts", createError);
    const bool refused = !Project::Create(project, sandbox / "Scripts", refusedError);
    const BuildProfiles fresh = BuildProfiles::Defaults(project);
    Check(created && refused && fs::is_directory(project / "Scenes", ec) && fs::is_directory(project / "Models", ec) &&
              fs::exists(project / "Scripts" / "VehicleDriver.luau", ec) && fs::exists(project / "Scripts" / "ChaseCamera.luau", ec) &&
              fs::exists(project / ".gitignore", ec) && fresh.profiles.size() == 2 && fresh.profiles[0].scene.empty(),
          "a new project", createError + (refused ? " | refused again: " + refusedError : " | made twice"));

    // The recent projects: the latest first, each once, gone folders left out.
    Project::RecentFile() = temp / "projects.json";
    const fs::path other = temp / "Other";
    fs::create_directories(other, ec);
    Project::Remember(project);
    Project::Remember(other);
    Project::Remember(project);
    Project::Remember(temp / "Gone");
    const std::vector<fs::path> recent = Project::Recent();
    std::string recentList;
    for (const fs::path& folder : recent)
        recentList += (recentList.empty() ? "" : ", ") + folder.filename().string();
    Check(recent.size() == 2 && fs::equivalent(recent[0], project, ec) && fs::equivalent(recent[1], other, ec), "recent projects", recentList);
    fs::remove_all(temp, ec);

    std::printf("%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}
