#pragma once
#include <Mariana/Export/BuildProfile.hpp>

#include <filesystem>
#include <string>
#include <vector>

// What a game takes from its project, as paths relative to the project folder (folders whole):
// the profile's scenes and every file they list as dependencies (a .gltf with its folder, its
// buffers and images; a .glb with its extracted .materials folder), the project's Physics.json,
// the pipeline and icon when they are the project's own, and its whole Scripts folder (presets
// put scripts no scene names). With `wholeProject`, everything but the editor's and the builds'
// folders. Files the engine has (its pipelines, shaders) aren't the project's to package.
struct GameFiles
{
    std::vector<std::filesystem::path> files;
    std::vector<std::string> warnings; // a dependency that is nowhere, say
};

namespace GameExport
{
    // False (and `error`) when the profile's scene isn't in the project.
    bool CollectFiles(const std::filesystem::path& project, const BuildProfile& profile, GameFiles& out, std::string& error);

    // The Player.json the game runs: the project's own (its spawns and camera) with the profile's
    // window, scene, pipeline, follow camera and profiling over it.
    std::string PlayerConfig(const std::filesystem::path& project, const BuildProfile& profile);

    // Copies the files into `folder` (emptied first) and writes `playerConfig` there as Player.json.
    bool StageFiles(const std::filesystem::path& project, const GameFiles& files, const std::string& playerConfig, const std::filesystem::path& folder,
                    std::string& error);
}
