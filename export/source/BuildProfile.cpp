#include <Mariana/Export/BuildProfile.hpp>

#include <json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

using Json = nlohmann::ordered_json;

namespace
{
    const char* const kFile = "BuildProfiles.json";

    Json Write(const BuildProfile& p)
    {
        Json j = Json::object();
        j["name"] = p.name;
        j["platform"] = BuildProfile::PlatformName(p.platform);
        j["configuration"] = BuildProfile::ConfigurationName(p.configuration);
        j["output"] = p.output;
        j["gameName"] = p.gameName;
        j["window"] = { { "title", p.title }, { "width", p.width }, { "height", p.height }, { "icon", p.icon } };
        j["scene"] = p.scene;
        j["scenes"] = p.scenes;
        j["pipeline"] = p.pipeline;
        j["followCamera"] = p.followCamera;
        j["logProfile"] = p.logProfile;
        j["wholeProject"] = p.wholeProject;
        return j;
    }

    BuildProfile Read(const Json& j)
    {
        BuildProfile p;
        p.name = j.value("name", p.name);
        p.platform = j.value("platform", std::string()) == "Web" ? BuildPlatform::Web : BuildPlatform::Windows;
        p.configuration = j.value("configuration", std::string()) == "Debug" ? BuildConfiguration::Debug : BuildConfiguration::Release;
        p.output = j.value("output", p.output);
        p.gameName = j.value("gameName", p.gameName);
        if (const auto window = j.find("window"); window != j.end() && window->is_object())
        {
            p.title = window->value("title", p.title);
            p.width = window->value("width", p.width);
            p.height = window->value("height", p.height);
            p.icon = window->value("icon", p.icon);
        }
        p.scene = j.value("scene", p.scene);
        if (const auto scenes = j.find("scenes"); scenes != j.end() && scenes->is_array())
            for (const Json& s : *scenes)
                if (s.is_string())
                    p.scenes.push_back(s.get<std::string>());
        p.pipeline = j.value("pipeline", p.pipeline);
        p.followCamera = j.value("followCamera", p.followCamera);
        p.logProfile = j.value("logProfile", p.logProfile);
        p.wholeProject = j.value("wholeProject", p.wholeProject);
        return p;
    }
}

std::filesystem::path BuildProfile::OutputFolder(const std::filesystem::path& project) const
{
    const std::filesystem::path folder = output.empty() ? std::filesystem::path("Builds") / name : std::filesystem::path(output);
    return (folder.is_absolute() ? folder : project / folder).lexically_normal();
}

const char* BuildProfile::PlatformName(BuildPlatform platform)
{
    return platform == BuildPlatform::Web ? "Web" : "Windows";
}

const char* BuildProfile::ConfigurationName(BuildConfiguration configuration)
{
    return configuration == BuildConfiguration::Debug ? "Debug" : "Release";
}

BuildProfile* BuildProfiles::Find(std::string_view name)
{
    for (BuildProfile& p : profiles)
        if (p.name == name)
            return &p;
    return nullptr;
}

BuildProfile* BuildProfiles::Active()
{
    if (BuildProfile* found = Find(active))
        return found;
    return profiles.empty() ? nullptr : &profiles.front();
}

std::string BuildProfiles::UniqueName(const std::string& base) const
{
    auto taken = [&](const std::string& name) {
        return std::any_of(profiles.begin(), profiles.end(), [&](const BuildProfile& p) { return p.name == name; });
    };
    std::string name = base;
    for (int i = 2; taken(name); ++i)
        name = base + " " + std::to_string(i);
    return name;
}

BuildProfiles BuildProfiles::Defaults(const std::filesystem::path& project)
{
    // The project's first scene (by name), Playground's when it has one.
    std::vector<std::string> scenes;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(project / "Scenes", ec))
        if (entry.path().extension() == ".scene")
            scenes.push_back(entry.path().stem().string());
    std::sort(scenes.begin(), scenes.end());
    std::string scene = scenes.empty() ? std::string() : scenes.front();
    if (std::find(scenes.begin(), scenes.end(), "Playground") != scenes.end())
        scene = "Playground";

    BuildProfiles out;
    BuildProfile windows;
    windows.name = "Windows";
    windows.gameName = scene.empty() ? std::string("Game") : scene;
    windows.title = windows.gameName;
    windows.scene = scene.empty() ? std::string() : "/Scenes/" + scene + ".scene";
    BuildProfile web = windows;
    web.name = "Web";
    web.platform = BuildPlatform::Web;
    out.profiles = { windows, web };
    out.active = windows.name;
    return out;
}

std::string BuildProfiles::ToJson() const
{
    Json j = Json::object();
    j["active"] = active;
    Json list = Json::array();
    for (const BuildProfile& p : profiles)
        list.push_back(Write(p));
    j["profiles"] = list;
    return j.dump(2);
}

BuildProfiles BuildProfiles::FromJson(const std::string& text, std::string& error)
{
    BuildProfiles out;
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object())
    {
        error = "not JSON";
        return out;
    }
    out.active = j.value("active", std::string());
    if (const auto list = j.find("profiles"); list != j.end() && list->is_array())
        for (const Json& p : *list)
            if (p.is_object())
                out.profiles.push_back(Read(p));
    return out;
}

bool BuildProfiles::Load(const std::filesystem::path& project, BuildProfiles& out, std::string& error)
{
    const std::filesystem::path file = project / kFile;
    std::error_code ec;
    if (!std::filesystem::exists(file, ec))
    {
        out = Defaults(project);
        return true;
    }
    std::ifstream stream(file, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    std::string problem;
    BuildProfiles read = FromJson(text.str(), problem);
    if (!problem.empty())
    {
        error = file.string() + ": " + problem;
        return false;
    }
    out = std::move(read);
    return true;
}

bool BuildProfiles::Save(const std::filesystem::path& project, std::string& error) const
{
    const std::filesystem::path file = project / kFile;
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "Could not write " + file.string();
        return false;
    }
    stream << ToJson() << "\n";
    return static_cast<bool>(stream);
}
