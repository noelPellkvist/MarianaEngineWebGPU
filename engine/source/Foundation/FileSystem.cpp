#include <Mariana/Foundation/FileSystem.hpp>

#include <algorithm>
#include <cctype>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

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

    std::string Normalized(const std::filesystem::path& path)
    {
        std::error_code error;
        const std::filesystem::path absolute = std::filesystem::absolute(path, error);
        return (error ? path : absolute).lexically_normal().generic_string();
    }

    // Paths compare without case on Windows, as its file system does.
    bool StartsWith(const std::string& text, const std::string& prefix)
    {
        if (text.size() < prefix.size())
            return false;
#ifdef _WIN32
        return std::equal(prefix.begin(), prefix.end(), text.begin(), [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
#else
        return text.compare(0, prefix.size(), prefix) == 0;
#endif
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

    std::string ToResourcePath(const std::filesystem::path& file)
    {
        const std::string full = Normalized(file);
        for (const std::filesystem::path& root : Roots())
        {
            std::string prefix = Normalized(root);
            if (!prefix.empty() && prefix.back() != '/')
                prefix += '/';
            if (StartsWith(full, prefix) && full.size() > prefix.size())
                return "/" + full.substr(prefix.size());
        }
        return {};
    }

    std::filesystem::path ExecutableDirectory()
    {
#if defined(_WIN32)
        std::wstring path(MAX_PATH, L'\0');
        DWORD length = 0;
        while ((length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()))) >= path.size())
            path.resize(path.size() * 2);
        path.resize(length);
        return std::filesystem::path(path).parent_path();
#elif defined(__EMSCRIPTEN__)
        return "/";
#else
        std::error_code error;
        const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", error);
        return error ? std::filesystem::current_path() : self.parent_path();
#endif
    }
}
