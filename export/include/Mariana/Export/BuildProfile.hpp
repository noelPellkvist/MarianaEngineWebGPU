#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

enum class BuildPlatform { Windows, Web };
enum class BuildConfiguration { Release, Debug };

// How a project is built into a game: for which platform and configuration, from which scenes, into
// which folder, and the window it opens. A project keeps its profiles in its BuildProfiles.json:
//   { "active": "Windows",
//     "profiles": [ { "name": "Windows", "platform": "Windows", "configuration": "Release",
//                     "output": "Builds/Windows", "gameName": "Playground",
//                     "window": { "title": "Playground", "width": 1280, "height": 720, "icon": "" },
//                     "scene": "/Scenes/Playground.scene", "scenes": [ "/Scenes/Other.scene" ],
//                     "pipeline": "", "followCamera": true, "logProfile": false,
//                     "wholeProject": false } ] }
// `output` is relative to the project (or absolute; empty = Builds/<name>); `scene`, `scenes`,
// `pipeline` (empty = the scene's) and `icon` are resource paths. Missing values keep defaults.
struct BuildProfile
{
    std::string name = "Windows";
    BuildPlatform platform = BuildPlatform::Windows;
    BuildConfiguration configuration = BuildConfiguration::Release;
    std::string output;
    std::string gameName = "Game"; // the executable's name on Windows
    std::string title;             // the window's; empty = gameName
    int width = 1280;
    int height = 720;
    std::string icon;
    std::string scene;               // the one the game starts in
    std::vector<std::string> scenes; // more scenes packaged with it
    std::string pipeline;
    bool followCamera = true; // a scene without a camera gets one chasing what the keys drive
    bool logProfile = false;  // the player logs its frame times every 5 s
    bool wholeProject = false; // every file of the project, not only what the scenes use

    std::filesystem::path OutputFolder(const std::filesystem::path& project) const;
    static const char* PlatformName(BuildPlatform platform);
    static const char* ConfigurationName(BuildConfiguration configuration);
};

struct BuildProfiles
{
    std::vector<BuildProfile> profiles;
    std::string active;

    BuildProfile* Find(std::string_view name);
    // The active one, else the first; nullptr when there are none.
    BuildProfile* Active();
    // A name no profile has yet ("Web", "Web 2", ...).
    std::string UniqueName(const std::string& base) const;

    // The project's BuildProfiles.json; a project without one gets Defaults. False (and `error`)
    // when the file is there but can't be read.
    static bool Load(const std::filesystem::path& project, BuildProfiles& out, std::string& error);
    bool Save(const std::filesystem::path& project, std::string& error) const;
    // A Windows and a Web profile starting in the project's first scene, named after it.
    static BuildProfiles Defaults(const std::filesystem::path& project);
    std::string ToJson() const;
    static BuildProfiles FromJson(const std::string& text, std::string& error);
};
