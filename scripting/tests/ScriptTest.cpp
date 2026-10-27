// MarianaScriptingTest: runs the script scenarios (Scenarios.hpp) natively and prints what each saw.
// A console program: no window, no GPU. Exits 1 when one fails.
#include "Scenarios.hpp"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <crtdbg.h>
#endif

int main()
{
#ifdef _WIN32
    // A failed check prints and exits instead of opening a dialog (it runs unattended).
    for (int type : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
    {
        _CrtSetReportMode(type, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
    }
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    PhysicsWorld::RegisterComponents();
    ScriptWorld::RegisterComponents();
    std::printf("MarianaScriptingTest\n");

    const std::vector<ScriptScenarios::Result> results = ScriptScenarios::RunAll([](const std::string& line) {
        std::printf("%s\n", line.c_str());
        std::fflush(stdout);
    });

    size_t failed = 0;
    for (const ScriptScenarios::Result& r : results)
        failed += r.passed ? 0 : 1;
    std::printf("%zu scenarios, %zu failed\n", results.size(), failed);
    return failed == 0 ? 0 : 1;
}
