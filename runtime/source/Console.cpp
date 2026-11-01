// Kept apart: <windows.h> clashes with the engine's headers.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

void DetachOwnConsole()
{
#ifdef _WIN32
    // The only process on its console: Windows made the console for it.
    DWORD processes[2];
    if (GetConsoleProcessList(processes, 2) == 1)
        FreeConsole();
#endif
}
