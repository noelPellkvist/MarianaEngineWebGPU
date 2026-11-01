#include <Player/PlayerApp.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Importers/SceneFile.hpp>

#include <cstdio>
#include <filesystem>
#include <string>

// Console.cpp: an exported game started from Explorer drops the console window Windows opened for
// it (started from a terminal, it keeps the terminal and the log in it).
void DetachOwnConsole();

namespace
{
    // Where the game's files are. An exported game carries them: data/game and data/engine next to
    // the exe natively, /game in the page's file system on the web (the web runtime's game.data).
    // Otherwise the project the player was built for.
    bool FindGame()
    {
        std::error_code ec;
        const std::filesystem::path data = FileSystem::ExecutableDirectory() / "data";
        if (std::filesystem::exists(data / "game" / "Player.json", ec))
        {
            FileSystem::SetEngineRoot(data / "engine");
            FileSystem::AddRoot(data / "game");
            return true;
        }
#ifdef __EMSCRIPTEN__
        if (std::filesystem::exists("/game/Player.json", ec))
        {
            FileSystem::AddRoot("/game");
            return true;
        }
#endif
        FileSystem::AddRoot(MARIANA_PROJECT_DIR);
        return false;
    }

    // --check: whether everything the game opens with is found where it looks (its scene and every
    // file the scene uses, the pipeline, the engine's shaders), without opening a window.
    int Check(const PlayerConfig& config)
    {
        int missing = 0;
        auto need = [&](const std::string& path, const char* what) {
            if (path.empty() || path.rfind("builtin://", 0) == 0)
                return;
            const std::filesystem::path found = FileSystem::Resolve(path);
            std::printf("%s %s %s\n", found.empty() ? "MISSING" : "ok     ", what, path.c_str());
            missing += found.empty() ? 1 : 0;
        };
        for (const std::filesystem::path& root : FileSystem::Roots())
            std::printf("root    %s\n", root.string().c_str());
        need("/Player.json", "config ");
        need("/Shaders/test.wgsl", "engine ");
        need(config.pipeline.empty() ? std::string("/Pipelines/Default.pipeline") : config.pipeline, "pipeline");
        need(config.window.iconPath, "icon   ");
        if (!config.scene.empty())
        {
            need(config.scene, "scene  ");
            std::string error;
            for (const std::string& dependency : SceneFile::Dependencies(FileSystem::Resolve(config.scene), error))
                need(dependency, "uses   ");
            if (!error.empty())
                std::printf("MISSING %s\n", error.c_str());
        }
        std::printf("%s\n", missing == 0 ? "Everything the game uses is there" : (std::to_string(missing) + " files missing").c_str());
        return missing == 0 ? 0 : 1;
    }
}

int main(int argc, char** argv)
{
    const bool exported = FindGame();
    const bool check = argc > 1 && std::string(argv[1]) == "--check";
    if (exported && !check)
        DetachOwnConsole();

    PlayerConfig config;
    std::string error;
    if (!PlayerConfig::Load("/Player.json", config, error))
        Logger::Error("Could not read the project's Player.json, running an empty scene: " + error);
    if (check)
        return Check(config);

    PlayerApp app(config);
    app.Start();
    return 0;
}
