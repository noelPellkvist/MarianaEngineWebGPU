// MarianaPhysicsTest: runs the physics scenarios (Scenarios.hpp) natively, on Jolt's thread pool,
// and prints what each measured. A console program: no window, no GPU. Exits 1 when one fails.
#include "Scenarios.hpp"

#include <Mariana/Rendering/SceneRenderer.hpp>

#include <cstdio>
#include <thread>

int main()
{
    PhysicsWorld::RegisterComponents();
    SceneRenderer::RegisterComponents(); // MeshComponent, for soft bodies
    std::printf("MarianaPhysicsTest: Jolt's thread pool, %u hardware threads\n", std::thread::hardware_concurrency());

    const std::vector<PhysicsScenarios::Result> results = PhysicsScenarios::RunAll([](const std::string& line) {
        std::printf("%s\n", line.c_str());
        std::fflush(stdout);
    });

    size_t failed = 0;
    for (const PhysicsScenarios::Result& r : results)
        failed += r.passed ? 0 : 1;
    std::printf("%zu scenarios, %zu failed\n", results.size(), failed);
    return failed == 0 ? 0 : 1;
}
