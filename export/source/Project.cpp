#include <Mariana/Export/Project.hpp>

#include <json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace
{
    fs::path DefaultRecentFile()
    {
#if defined(__EMSCRIPTEN__)
        return {};
#elif defined(_WIN32)
        if (const wchar_t* appData = _wgetenv(L"APPDATA"); appData && *appData)
            return fs::path(appData) / "Mariana" / "projects.json";
        return {};
#else
        if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && *config)
            return fs::path(config) / "mariana" / "projects.json";
        if (const char* home = std::getenv("HOME"); home && *home)
            return fs::path(home) / ".config" / "mariana" / "projects.json";
        return {};
#endif
    }

    // Every folder in the list, as written (gone ones too).
    std::vector<fs::path> ReadList()
    {
        std::vector<fs::path> folders;
        std::ifstream stream(Project::RecentFile(), std::ios::binary);
        if (!stream)
            return folders;
        std::stringstream text;
        text << stream.rdbuf();
        const Json json = Json::parse(text.str(), nullptr, false);
        if (!json.is_object() || !json.contains("projects") || !json["projects"].is_array())
            return folders;
        for (const Json& entry : json["projects"])
            if (entry.is_string())
            {
                const std::string text = entry.get<std::string>();
                folders.emplace_back(std::u8string(text.begin(), text.end()));
            }
        return folders;
    }

    bool IsFolder(const fs::path& folder)
    {
        std::error_code ec;
        return fs::is_directory(folder, ec);
    }

    std::string Text(const fs::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }
}

namespace Project
{
    const std::vector<std::string>& StarterScripts()
    {
        static const std::vector<std::string> scripts = { "VehicleDriver.luau", "CharacterDriver.luau", "ChaseCamera.luau" };
        return scripts;
    }

    bool Create(const fs::path& folder, const fs::path& starterScripts, std::string& error)
    {
        std::error_code ec;
        if (folder.empty())
        {
            error = "No folder given for the project";
            return false;
        }
        if (fs::exists(folder, ec))
        {
            if (!fs::is_directory(folder, ec))
            {
                error = Text(folder) + " is a file, not a folder";
                return false;
            }
            if (!fs::is_empty(folder, ec))
            {
                error = Text(folder) + " isn't empty: a new project needs an empty or new folder";
                return false;
            }
        }
        for (const char* sub : { "Scenes", "Models", "Textures", "Materials", "Scripts" })
        {
            fs::create_directories(folder / sub, ec);
            if (ec)
            {
                error = "Could not make " + Text(folder / sub) + ": " + ec.message();
                return false;
            }
        }
        if (!starterScripts.empty())
            for (const std::string& script : StarterScripts())
                if (fs::exists(starterScripts / script, ec))
                    fs::copy_file(starterScripts / script, folder / "Scripts" / script, fs::copy_options::skip_existing, ec);
        std::ofstream ignore(folder / ".gitignore", std::ios::binary);
        ignore << "# The editor's caches (thumbnails, the editor camera, script types) and its build tool paths\n"
                  ".mariana/\n"
                  "# Games built from the project (File > Build, MarianaExport)\n"
                  "Builds/\n";
        if (!ignore)
        {
            error = "Could not write " + Text(folder / ".gitignore");
            return false;
        }
        return true;
    }

    std::vector<fs::path> Recent()
    {
        std::vector<fs::path> folders = ReadList();
        folders.erase(std::remove_if(folders.begin(), folders.end(), [](const fs::path& f) { return !IsFolder(f); }), folders.end());
        return folders;
    }

    void Remember(const fs::path& folder)
    {
        const fs::path file = RecentFile();
        if (file.empty() || folder.empty())
            return;
        std::error_code ec;
        const fs::path absolute = fs::weakly_canonical(fs::absolute(folder, ec), ec);
        std::vector<fs::path> folders = ReadList();
        folders.erase(std::remove_if(folders.begin(), folders.end(), [&](const fs::path& f) {
                          std::error_code same;
                          return f == absolute || fs::equivalent(f, absolute, same);
                      }),
                      folders.end());
        folders.insert(folders.begin(), absolute);
        if (folders.size() > 12)
            folders.resize(12);
        Json list = Json::array();
        for (const fs::path& f : folders)
            list.push_back(Text(f));
        fs::create_directories(file.parent_path(), ec);
        std::ofstream stream(file, std::ios::binary);
        stream << Json{ { "projects", list } }.dump(2) << "\n";
    }

    fs::path& RecentFile()
    {
        static fs::path file = DefaultRecentFile();
        return file;
    }
}
