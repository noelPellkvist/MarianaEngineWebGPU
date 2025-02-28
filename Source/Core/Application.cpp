#include "Application.hpp"
#include "Logging.hpp"

#include <webgpu/webgpu_cpp.h>

#include <iostream>

Application::Application() : m_Window(1336, 768, "MARIANA"), m_RenderLayer(m_Window.GetTargetFormat())
{
}

Application::~Application()
{
}

void Application::Render()
{
  wgpu::SurfaceTexture surfaceTexture;
  m_Window.GetCurrentTexture(&surfaceTexture);

  m_RenderLayer.Render(surfaceTexture);
}

void Application::Start()
{
  Logging::PrintSuccess("Start method called in application");
  while (!m_Window.ShouldClose())
  {
    m_Window.PollEvents();
    Update();
    m_Window.Present();
    instance.ProcessEvents();
  }
}

void Application::Update()
{
  Render();
}