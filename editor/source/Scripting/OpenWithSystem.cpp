// Kept apart: <windows.h> clashes with the engine's headers.
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

namespace ScriptEditor
{
    void OpenWithSystem(const std::filesystem::path& file)
    {
#ifdef _WIN32
        ShellExecuteW(nullptr, L"open", file.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
        (void)file;
#endif
    }
}
