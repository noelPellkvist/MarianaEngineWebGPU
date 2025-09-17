#include <Application.hpp>
#include <Init.hpp>

#include <chrono>

Application::Application(const std::string& name)
    : m_Name(name), m_Running(false), m_Window(1366, 768, name), input(m_Window.GetWindow()), renderpass(true, true, windowFormat, 1366, 768), cam(input)
{
    Logger::Info("Application Created: " + m_Name);
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
    while (!m_Window.ShouldClose())
    {
        MainLoop();
    }
    Logger::Info("Application Shutting down...");
    OnShutdown();
    Shutdown();
}

void Application::Initalize()
{
    Init();
    glm::vec3 eye    = {0.0f, 0.0f, 0.0f};
    glm::vec3 target = {0.0f, 0.0f, 1.0f};

    cam.SetPosition(eye);
    cam.SetYawPitch(glm::half_pi<float>(), 0.0f);

    m_Window.GetSurface();
}

void Application::MainLoop()
{
    using clock = std::chrono::steady_clock;
    static auto t0 = clock::now();
    float dt = std::chrono::duration<float>(clock::now() - t0).count();
    t0 = clock::now();

    //first thing that happens in the loop is we get the time at the start of the frame
    OnUpdate(dt);
    cam.Update(dt);

    //After everything is updated we render
    OnRender();

    input.Update();
    surface.Present();
    instance.ProcessEvents();
}

void Application::Shutdown()
{

}