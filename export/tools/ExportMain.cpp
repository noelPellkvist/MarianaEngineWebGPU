// MarianaExport: builds a project's build profile into a game from the command line.
//   MarianaExport --project <folder> [--profile <name>] [--list]
//                 [--native-build <folder>] [--web-build <folder>] [--emsdk <folder>]
// Without --profile the project's active one. --list prints the files the game would take and its
// Player.json, and builds nothing. The build folders and the SDK default to the project's
// .mariana/build-tools.json, else to where this tool was built from and EMSDK.
#include <Mariana/Export/GameExporter.hpp>

#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    std::filesystem::path project;
    std::string profileName;
    bool list = false;
    std::filesystem::path nativeBuild, webBuild, emsdk;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto value = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (arg == "--project")
            project = value();
        else if (arg == "--profile")
            profileName = value();
        else if (arg == "--list")
            list = true;
        else if (arg == "--native-build")
            nativeBuild = value();
        else if (arg == "--web-build")
            webBuild = value();
        else if (arg == "--emsdk")
            emsdk = value();
        else
        {
            std::fprintf(stderr, "Unknown argument %s\n", arg.c_str());
            return 2;
        }
    }
    if (project.empty())
    {
        std::fprintf(stderr, "Usage: MarianaExport --project <folder> [--profile <name>] [--list]\n");
        return 2;
    }
    project = std::filesystem::absolute(project).lexically_normal();

    BuildProfiles profiles;
    std::string error;
    if (!BuildProfiles::Load(project, profiles, error))
    {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const BuildProfile* profile = profileName.empty() ? profiles.Active() : profiles.Find(profileName);
    if (!profile)
    {
        std::fprintf(stderr, "No profile %s in %s\n", profileName.c_str(), (project / "BuildProfiles.json").string().c_str());
        return 1;
    }

    if (list)
    {
        GameFiles files;
        if (!GameExport::CollectFiles(project, *profile, files, error))
        {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        for (const std::filesystem::path& file : files.files)
            std::printf("%s\n", file.generic_string().c_str());
        for (const std::string& warning : files.warnings)
            std::printf("warning: %s\n", warning.c_str());
        std::printf("Player.json:\n%s", GameExport::PlayerConfig(project, *profile).c_str());
        return 0;
    }

    BuildTools tools = BuildTools::Load(project);
    if (!nativeBuild.empty())
        tools.nativeBuild = std::filesystem::absolute(nativeBuild);
    if (!webBuild.empty())
        tools.webBuildRelease = tools.webBuildDebug = std::filesystem::absolute(webBuild);
    if (!emsdk.empty())
        tools.emsdk = std::filesystem::absolute(emsdk);

    GameExporter exporter(project, *profile, tools);
    exporter.onLine = [](const GameExporter::Line& line) {
        std::FILE* out = line.error ? stderr : stdout;
        std::fprintf(out, "%s\n", line.text.c_str());
        std::fflush(out);
    };
    return exporter.Run() ? 0 : 1;
}
