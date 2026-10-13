#include <Mariana/Foundation/FileSystem.hpp>

#include <system_error>

namespace
{
    std::vector<std::filesystem::path>& AppRoots()
    {
        static std::vector<std::filesystem::path> roots;
        return roots;
    }

    std::filesystem::path& EngineRoot()
    {
        static std::filesystem::path root = MARIANA_ENGINE_RESOURCES;
        return root;
    }

    // "/Shaders/x.wgsl" -> "Shaders/x.wgsl", so it joins under a root on every platform.
    std::filesystem::path Relative(const std::string& path)
    {
        const size_t start = path.find_first_not_of("/\\");
        return start == std::string::npos ? std::filesystem::path() : std::filesystem::path(path.substr(start));
    }
}

namespace FileSystem
{
    void AddRoot(const std::filesystem::path& folder)
    {
        AppRoots().insert(AppRoots().begin(), folder);
    }

    void SetEngineRoot(const std::filesystem::path& folder)
    {
        EngineRoot() = folder;
    }

    std::vector<std::filesystem::path> Roots()
    {
        std::vector<std::filesystem::path> roots = AppRoots();
        roots.push_back(EngineRoot());
        return roots;
    }

    std::filesystem::path Resolve(const std::string& path)
    {
        const std::filesystem::path relative = Relative(path);
        std::error_code error;
        for (const std::filesystem::path& root : Roots())
        {
            std::filesystem::path full = root / relative;
            if (std::filesystem::exists(full, error))
                return full;
        }
        return {};
    }

    std::filesystem::path ResolveForWrite(const std::string& path)
    {
        if (std::filesystem::path existing = Resolve(path); !existing.empty())
            return existing;
        return Roots().front() / Relative(path);
    }
}
