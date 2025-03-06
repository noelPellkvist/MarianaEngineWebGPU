#include "Application.hpp"
#include "Logging.hpp"

#include <webgpu/webgpu_cpp.h>

#include <iostream>
#include "Resources.hpp"

Application::Application() : m_Window(1336, 768, "MARIANA"), scene("built_in_scene", m_Window.GetTargetFormat())
{
  auto c = scene.CreateGameobject("Cube");
  scene.GetEntities().emplace<Mesh>(c, Resources::LoadObjMesh("cube.obj", scene.GetShaders()[0]));
}

Application::~Application()
{
}

void Application::Render()
{
  wgpu::SurfaceTexture surfaceTexture;
  m_Window.GetCurrentTexture(&surfaceTexture);

  scene.DrawAllObjects(surfaceTexture);
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