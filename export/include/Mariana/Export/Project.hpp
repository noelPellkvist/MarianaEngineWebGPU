#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Project folders. A project is a folder with a game's files (scenes, models, textures, scripts)
// that the editor opens and builds games from; any folder will do. A new one starts with the
// folders those files go in, a .gitignore for what the editor and builds write into it, and the
// driver scripts the editor's ready-made vehicles and characters are given (Create > Vehicle,
// Create > Physics > Character attach them by resource path).
namespace Project
{
    // The scripts a new project gets from `starterScripts` (the Sandbox's Scripts folder).
    const std::vector<std::string>& StarterScripts();

    // Makes a project at `folder`, which may exist but must be empty: Scenes, Models, Textures,
    // Materials and Scripts (with the starter scripts copied from `starterScripts` when that is
    // given) and a .gitignore. False (and `error`) when the folder isn't empty or can't be written.
    bool Create(const std::filesystem::path& folder, const std::filesystem::path& starterScripts, std::string& error);

    // The projects the editor opened, the latest first, those whose folders are gone left out. Kept
    // in the user's settings (RecentFile); on the web there are none.
    std::vector<std::filesystem::path> Recent();
    // Puts `folder` first in that list (at most 12 are kept).
    void Remember(const std::filesystem::path& folder);
    // Where the list is kept: %APPDATA%/Mariana/projects.json on Windows, else
    // $XDG_CONFIG_HOME/mariana or ~/.config/mariana; empty on the web. Tests point it elsewhere.
    std::filesystem::path& RecentFile();
}
