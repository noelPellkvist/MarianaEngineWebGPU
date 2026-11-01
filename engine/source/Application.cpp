#include <Mariana/Application.hpp>
#include "Graphics/Init.hpp"
#include "Graphics/GpuTimer.hpp"
#include <Mariana/Foundation/Profiler.hpp>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
#include <chrono>

Application::Application(const WindowSettings& window)
    : m_Running(false), m_Window(window.width, window.height, window.title, window.iconPath), input(m_Window.GetWindow())
{
    Profiler::SetMainThread();
    Init();
    // Configure the surface now so derived apps can build pipelines for the window's format.
    m_Window.GetSurface();
    Logger::Info("Application Created");
}

Application::~Application()
{
}

void Application::Start()
{
    Logger::Info("Application Starting...");
    Initalize();
    Logger::Info("Application Started.");
    OnStart();
    m_Running = true;
    #if defined(__EMSCRIPTEN__)
        emscripten_set_main_loop_arg([](void* arg) {
                Profiler::BeginFrame();
                static_cast<Application*>(arg)->MainLoop();
                GpuTimer::EndFrame(Profiler::FrameIndex());
                Profiler::EndFrame();
            }, this, 0, true);
    #else
    while (m_Running)
    {
        if (m_Window.ShouldClose())
        {
            if (OnCloseRequested())
                break;
            m_Window.CancelClose();
        }
        Profiler::BeginFrame();
        MainLoop();
        {
            // Waits for the display (vsync), so this is time spent waiting rather than working.
            MARIANA_PROFILE("Present");
            surface.Present();
            instance.ProcessEvents();
        }
        GpuTimer::EndFrame(Profiler::FrameIndex());
        Profiler::EndFrame();
    }
    #endif
    Logger::Info("Application Shutting down...");
    OnShutdown();
    Shutdown();
    
}

void Application::Initalize()
{
}

void Application::Quit()
{
    m_Running = false;
}

void Application::MainLoop()
{
    using clock = std::chrono::steady_clock;
    static auto t0 = clock::now();
    float dt = std::chrono::duration<float>(clock::now() - t0).count();
    t0 = clock::now();

    m_Window.FollowCanvas();
    gameClock.Tick(dt);

    {
        MARIANA_PROFILE("Update");
        OnUpdate(dt);
        if (cam)
        {
            cam->OnUpdate(dt);
            cam->UpdateBuffer();
        }
    }
    {
        MARIANA_PROFILE("GUI");
        OnGUI();
    }
    {
        // After everything is updated we render.
        MARIANA_PROFILE("Render");
        OnRender();
    }

    input.Update();
    
}

void Application::Shutdown()
{
}