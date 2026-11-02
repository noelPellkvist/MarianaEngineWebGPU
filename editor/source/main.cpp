#include <Editor/EditorApp.hpp>
#include <Mariana/Export/Project.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace
{
    // The project to open: --project <folder>, else the one opened last, else the Sandbox. On the web
    // always the Sandbox, the project the page holds.
    fs::path ChooseProject(int argc, char** argv)
    {
#if !defined(__EMSCRIPTEN__)
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (std::string(argv[i]) != "--project")
                continue;
            std::error_code ec;
            fs::path folder = fs::absolute(argv[i + 1], ec).lexically_normal();
            if (!folder.has_filename())
                folder = folder.parent_path();
            if (fs::is_directory(folder, ec))
                return folder;
            Logger::Error(std::string("No project folder ") + argv[i + 1]);
        }
        if (const std::vector<fs::path> recent = Project::Recent(); !recent.empty())
            return recent.front();
#else
        (void)argc;
        (void)argv;
#endif
        return MARIANA_PROJECT_DIR;
    }
}

int main(int argc, char** argv)
{
    const fs::path project = ChooseProject(argc, argv);
    Project::Remember(project);

    // Resource paths are looked up in the project first, then the editor's resources, then the engine's.
    FileSystem::AddRoot(MARIANA_EDITOR_RESOURCES);
    FileSystem::AddRoot(project);

    EditorApp app(project);
    app.Start();
}
