#include <Mariana/Export/GameFiles.hpp>
#include <Mariana/Importers/SceneFile.hpp>

#include <json.hpp>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;

namespace
{
    // A resource path ("/Models/Car.glb") as a path under the project, or empty when it isn't one
    // (an engine resource, a builtin).
    fs::path InProject(const fs::path& project, const std::string& resource)
    {
        if (resource.empty() || resource[0] != '/' || resource.rfind("builtin://", 0) == 0)
            return {};
        std::error_code ec;
        const fs::path relative = fs::path(resource.substr(1)).lexically_normal();
        return fs::exists(project / relative, ec) ? relative : fs::path();
    }

    bool Exists(const fs::path& path)
    {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    // Folders the editor and builds keep in a project, never part of a game.
    bool IsEditorFolder(const fs::path& name)
    {
        return name == ".mariana" || name == "Builds" || name == ".vscode" || name == ".git";
    }

    // Whether `path` is `folder` or inside it (both relative to the project).
    bool Within(const fs::path& path, const fs::path& folder)
    {
        auto p = path.begin();
        for (auto f = folder.begin(); f != folder.end(); ++f, ++p)
            if (p == path.end() || *p != *f)
                return false;
        return true;
    }
}

namespace GameExport
{
    bool CollectFiles(const fs::path& project, const BuildProfile& profile, GameFiles& out, std::string& error)
    {
        out = GameFiles{};
        std::set<fs::path> files;
        auto add = [&](const fs::path& relative) {
            if (!relative.empty())
                files.insert(relative);
        };
        // A model with what it needs: a .gltf's whole folder, a .glb and its extracted materials.
        auto addResource = [&](const std::string& resource) {
            const fs::path relative = InProject(project, resource);
            if (relative.empty())
                return;
            std::string extension = relative.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == ".gltf")
            {
                add(relative.parent_path());
                return;
            }
            if (extension == ".glb")
            {
                const fs::path materials = relative.parent_path() / (relative.stem().string() + ".materials");
                if (Exists(project / materials))
                    add(materials);
            }
            add(relative);
        };

        if (profile.wholeProject)
        {
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(project, ec))
            {
                const fs::path name = entry.path().filename();
                if (!IsEditorFolder(name) && name != "Player.json" && name != "BuildProfiles.json")
                    add(name);
            }
        }

        std::vector<std::string> scenes = { profile.scene };
        scenes.insert(scenes.end(), profile.scenes.begin(), profile.scenes.end());
        for (size_t i = 0; i < scenes.size(); ++i)
        {
            const std::string& scene = scenes[i];
            if (scene.empty())
                continue;
            const fs::path relative = InProject(project, scene);
            if (relative.empty())
            {
                if (i == 0)
                {
                    error = "The project has no " + scene;
                    return false;
                }
                out.warnings.push_back("The project has no " + scene);
                continue;
            }
            add(relative);
            std::string problem;
            for (const std::string& dependency : SceneFile::Dependencies(project / relative, problem))
            {
                if (InProject(project, dependency).empty() && dependency.rfind("builtin://", 0) != 0 && !dependency.empty() && dependency[0] == '/')
                {
                    // Not the project's: fine when the engine has it (a pipeline, say).
                    const fs::path engine = fs::path(MARIANA_ENGINE_RESOURCES) / dependency.substr(1);
                    if (!Exists(engine))
                        out.warnings.push_back(scene + " uses " + dependency + ", which is nowhere");
                }
                addResource(dependency);
            }
            if (!problem.empty())
                out.warnings.push_back(problem);
        }
        if (profile.scene.empty())
        {
            error = "The profile has no scene to start in";
            return false;
        }

        if (Exists(project / "Physics.json"))
            add("Physics.json");
        addResource(profile.pipeline);
        addResource(profile.icon);
        if (Exists(project / "Scripts"))
            add("Scripts");
        // Prefabs too (scripts spawn them by name), with the files they use.
        if (Exists(project / "Prefabs"))
        {
            add("Prefabs");
            std::error_code ec;
            for (const auto& entry : fs::recursive_directory_iterator(project / "Prefabs", ec))
            {
                if (entry.path().extension() != ".prefab")
                    continue;
                std::string problem;
                for (const std::string& dependency : SceneFile::Dependencies(entry.path(), problem))
                    addResource(dependency);
                if (!problem.empty())
                    out.warnings.push_back(problem);
            }
        }

        // A file inside a folder that is taken whole is taken with it.
        for (const fs::path& file : files)
        {
            const bool inside = std::any_of(files.begin(), files.end(), [&](const fs::path& folder) { return folder != file && Within(file, folder); });
            if (!inside)
                out.files.push_back(file);
        }
        return true;
    }

    std::string PlayerConfig(const fs::path& project, const BuildProfile& profile)
    {
        Json config = Json::object();
        if (std::ifstream stream(project / "Player.json", std::ios::binary); stream)
        {
            std::ostringstream text;
            text << stream.rdbuf();
            const Json read = Json::parse(text.str(), nullptr, false);
            if (read.is_object())
                config = read;
        }
        Json& window = config["window"];
        if (!window.is_object())
            window = Json::object();
        window["title"] = profile.title.empty() ? profile.gameName : profile.title;
        window["width"] = profile.width;
        window["height"] = profile.height;
        if (!profile.icon.empty())
            window["icon"] = profile.icon;
        config["scene"] = profile.scene;
        if (profile.pipeline.empty())
            config.erase("pipeline");
        else
            config["pipeline"] = profile.pipeline;
        Json& camera = config["camera"];
        if (!camera.is_object())
            camera = Json::object();
        camera["follow"] = profile.followCamera;
        config["profile"] = profile.logProfile;
        return config.dump(2) + "\n";
    }

    bool StageFiles(const fs::path& project, const GameFiles& files, const std::string& playerConfig, const fs::path& folder, std::string& error)
    {
        std::error_code ec;
        fs::remove_all(folder, ec);
        fs::create_directories(folder, ec);
        if (ec)
        {
            error = "Could not make " + folder.string() + ": " + ec.message();
            return false;
        }
        for (const fs::path& file : files.files)
        {
            const fs::path from = project / file;
            const fs::path to = folder / file;
            fs::create_directories(to.parent_path(), ec);
            if (fs::is_directory(from, ec))
            {
                fs::create_directories(to, ec);
                // Without the editor's caches (thumbnails) that may sit in it.
                for (auto it = fs::recursive_directory_iterator(from, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
                {
                    if (IsEditorFolder(it->path().filename()))
                    {
                        it.disable_recursion_pending();
                        continue;
                    }
                    const fs::path target = to / fs::relative(it->path(), from, ec);
                    if (it->is_directory(ec))
                        fs::create_directories(target, ec);
                    else
                        fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, ec);
                    if (ec)
                    {
                        error = "Could not copy " + it->path().string() + ": " + ec.message();
                        return false;
                    }
                }
            }
            else if (!fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec) || ec)
            {
                error = "Could not copy " + from.string() + ": " + ec.message();
                return false;
            }
        }
        std::ofstream stream(folder / "Player.json", std::ios::binary | std::ios::trunc);
        stream << playerConfig;
        if (!stream)
        {
            error = "Could not write " + (folder / "Player.json").string();
            return false;
        }
        return true;
    }
}
