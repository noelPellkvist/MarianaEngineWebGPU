#include <Application.hpp>

#include <chrono>

Application::Application(const std::string& name)
    : m_Name(name), m_Running(false), m_Window(1366, 768, name)
{
    Logger::Info("Application Created: " + m_Name);
}

Application::~Application()
{
}

void Application::Start()
{
    Logger::Info("Application Starting...");
    Init();
    m_Running = true;
    Logger::Info("Application Started.");
    OnStart();
    while (m_Window.ShouldClose() == false)
    {
        MainLoop();
    }
    Logger::Info("Application Shutting down...");
    OnShutdown();
    Shutdown();
}

void Application::Init()
{

}

void Application::MainLoop()
{
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();


    //first thing that happens in the loop is we get the time at the start of the frame
    OnUpdate(std::chrono::duration<float>(clock::now() - t0).count());

    //After everything is updated we render
    OnRender();
}

void Application::Shutdown()
{

}