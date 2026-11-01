// MarianaEngineTest: the engine's scenarios (the profiler; animation: root motion, events, IK; scene
// files) with known answers. A console program: no window, no GPU. Exits 1 when one fails.
#include "AnimationScenarios.hpp"
#include "ProfilerScenarios.hpp"
#include "SceneScenarios.hpp"

#include <Mariana/Animation/Animator.hpp>
#include <Mariana/Rendering/SceneRenderer.hpp>

#include <cstdio>
#include <cstdlib>
#include <vector>

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
    Profiler::SetMainThread();
    Animator::RegisterComponents();
    SceneRenderer::RegisterComponents();
    std::printf("MarianaEngineTest\n");

    const EngineScenarios::Log log = [](const std::string& line) {
        std::printf("%s\n", line.c_str());
        std::fflush(stdout);
    };
    std::vector<EngineScenarios::Result> results = { EngineScenarios::ProfilerScopes(log), EngineScenarios::ProfilerFrames(log),
                                                     EngineScenarios::RootMotionWalk(log), EngineScenarios::RootMotionTurn(log),
                                                     EngineScenarios::RootMotionBlend(log), EngineScenarios::AnimationEventsFire(log),
                                                     EngineScenarios::TwoBoneIKReach(log), EngineScenarios::LookAtTurn(log) };
    for (EngineScenarios::Result& r : EngineScenarios::SceneRoundTrip(log))
        results.push_back(std::move(r));
    results.push_back(EngineScenarios::SceneSignature(log));
    results.push_back(EngineScenarios::SceneKeepEntity(log));
    results.push_back(EngineScenarios::SceneFieldWrite(log));

    size_t failed = 0;
    for (const EngineScenarios::Result& r : results)
        failed += r.passed ? 0 : 1;
    std::printf("%zu scenarios, %zu failed\n", results.size(), failed);
    return failed == 0 ? 0 : 1;
}
